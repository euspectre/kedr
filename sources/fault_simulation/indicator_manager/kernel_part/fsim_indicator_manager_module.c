/*
 * Implements API for managing indicators functions.
 *
 * Also, implement kernel part of interaction process with user space
 * ('system call' to kedr_fsim_set_indicator_by_name())
 */

#include <kedr/fault_simulation/fsim_indicator_manager.h>
#include <kedr/fault_simulation/fsim_indicator_manager_internal.h> /* protocol of interaction with user space*/

#include <linux/module.h>
#include <linux/list.h> /* list functions */
#include <linux/slab.h> /* kmalloc & kfree */
#include <linux/types.h> /* atomic_t */
#include <linux/spinlock.h> /* spinlock for protect list of indicators*/
#include <linux/wait.h> /* for wait deleting of indicator */
#include <linux/sched.h> /* schedule */
/* weak reference of module functionality */
#include <kedr/module_weak_ref/module_weak_ref.h>

/* kedr_target_module_in_init */
#include <kedr/base/common.h>
/* implemetation of kernel-user spaces interaction */
#include <kedr/syscall_connector/syscall_connector.h>

#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

//interaction type for kedr_fsim_set_indicator_by_name() system call
static sc_interaction_id kedr_fsim_set_indicator_id = -1;

struct indicator_descriptor
{
	struct list_head list;

	const char* indicator_name;
	kedr_fsim_fault_indicator fi;
	const char* format_string;
	struct module* m;
	kedr_fsim_init_indicator_state init_state;
	kedr_fsim_destroy_indicator_state destroy_state;
    //control fields
    //because all fields except control - really constants, doesn't need spinlock
    //refcounting for concurrently deleting and using given indicator
    atomic_t refs;
    // waitqueue for wait until object may be destroyed.
    wait_queue_head_t wait_finish;
};
//initialize and destroy content of 'struct indicator_descriptor'
static void
indicator_descriptor_init(struct indicator_descriptor* indicator,
    const char* indicator_name,
	kedr_fsim_fault_indicator fi,
	const char* format_string,
	struct module* m,
	kedr_fsim_init_indicator_state init_state,
	kedr_fsim_destroy_indicator_state destroy_state);

static void
indicator_descriptor_destroy(struct indicator_descriptor* indicator);
//refcounting
static void indicator_descriptor_ref(struct indicator_descriptor* indicator);
static void indicator_descriptor_unref(struct indicator_descriptor* indicator);
//perform 'unref' and wait until last reference will be dropped, then call 'destroy'
static void indicator_descriptor_destroy_waiting(struct indicator_descriptor* indicator);

static struct list_head indicators = LIST_HEAD_INIT(indicators);
static spinlock_t indicators_spinlock;//protect 'indicators' list
// Auxiliary functions

// Look for indicator descriptor by its name.
// Should be executed under indicators_spinlock taken.
static struct indicator_descriptor*
indicator_descriptor_lookup(const char* indicator_name);

/*
 * Callback function, called when module which
 * registers indicator, is unloaded.
 */

static void on_module_unload(struct module* m,
	struct indicator_descriptor* node);
// Function for interact with user space
static void
communicate_set_indicator(const sc_interaction* interaction,
	const void* buf, size_t len, void* data);

static int indicator_manager_library_try_use(void);
static void indicator_manager_library_unuse(void);

// Common used indicator functions(them required by user-space library)

/*
 * Always simulate failure.
 */

static int
indicator_always_fault(void* indicator_state, void* user_data)
{
	(void)indicator_state;//unused
	(void)user_data;//unused
	return 1;
}

/*
 * Simulate failure always after module has initialized.
 */

static int
indicator_always_fault_after_init(void* indicator_state,
	void* user_data)
{
	(void)indicator_state;//unused
	(void)user_data;//unused
	return !kedr_target_module_in_init();
}

/*
 * Take size_t parameter (size_limit) when is set
 * for particular simulation point, and pointer to
 * size_t parameter (size) when need to simulate.
 *
 * Simulate failure if size > size_limit.
 */

static int
indicator_fault_if_size_greater(size_t* size_limit, size_t* size)
{
	return *size > *size_limit;
}
static int
indicator_fault_if_size_greater_init_state(const size_t* params,
	size_t params_len, size_t** state)
{
	//whether size of params buffer is correct
	if(params_len != sizeof(*params))
	{
		print_error("Size of payload %zu, expected size %zu.",
			params_len, sizeof(*params));
		return -2;
	}
	*state = kmalloc(sizeof(**state), GFP_KERNEL);
	**state = *params;
	return 0;
}
static void
indicator_fault_if_size_greater_destroy_state(size_t* state)
{
	kfree(state);
}

///////////////Implementation of exported functions///////////

// helper function for 'init_state'
const char** kedr_fsim_indicator_params_get_strings(const void* params,
    size_t params_len, int* argc)
{
    int i;
    const char** result = NULL;
    struct kedr_fsim_indicator_params_strings arr;
    if(kedr_fsim_indicator_params_strings_get(&arr, params, params_len))
        return NULL;
    //if argc == 0 return any not-NULL pointer.
    result = kmalloc(sizeof(*result) * (arr.argc ? arr.argc : 1), GFP_KERNEL);
    if(result == NULL)
    {
        print_error0("Cannot allocate memory for array of strings");
        return NULL;
    }
    for(i = 0; i < arr.argc; i++)
        result[i] = kedr_fsim_indicator_params_strings_get_string(&arr, i);

    *argc = arr.argc;
    return result;
}
EXPORT_SYMBOL(kedr_fsim_indicator_params_get_strings);

/*
 * Bind indicator_name with particular indicator functions.
 */

int kedr_fsim_indicator_function_register(const char* indicator_name,
	kedr_fsim_fault_indicator fi, const char* format_string,
	struct module* m,
	kedr_fsim_init_indicator_state init_state,
	kedr_fsim_destroy_indicator_state destroy_state)
{
	struct indicator_descriptor* new_node;
    unsigned long flags;

	if(*indicator_name == '\0')
	{
		print_error0("Indicator name \"\" is reserved for "
			"special use - clearing indicator.");
		return -1;
	}

	new_node = kmalloc(sizeof(*new_node), GFP_KERNEL);
	if(new_node == NULL)
	{
		print_error0("Cannot allocate new indicator descriptor.");
		return -2;
	}
    indicator_descriptor_init(new_node, indicator_name, fi, format_string, m,
        init_state, destroy_state);
	spin_lock_irqsave(&indicators_spinlock, flags);

	if(indicator_descriptor_lookup(indicator_name))
    {
        spin_unlock_irqrestore(&indicators_spinlock, flags);
        print_error("Indicator with name '%s' already registered.",
            indicator_name);
        indicator_descriptor_destroy(new_node);
        kfree(new_node);
        return -1;
    }

	list_add_tail(&new_node->list, &indicators);
    spin_unlock_irqrestore(&indicators_spinlock, flags);

	module_weak_ref(m, (destroy_notify)on_module_unload, new_node);

	return 0;
}
EXPORT_SYMBOL(kedr_fsim_indicator_function_register);

/*
 * Unregister indicator function.
 */

void kedr_fsim_indicator_function_unregister(const char* indicator_name)
{
	unsigned long flags;
	struct indicator_descriptor* node;
    int is_module_currently_unloaded;

    spin_lock_irqsave(&indicators_spinlock, flags);
	node = indicator_descriptor_lookup(indicator_name);
	if(node == NULL)
    {
        spin_unlock_irqrestore(&indicators_spinlock, flags);
        print_error("Indicator with name '%s' doesn't registered.",
            indicator_name);
        return;
    }

	is_module_currently_unloaded = module_weak_unref(node->m, (destroy_notify)on_module_unload, node);
    list_del(&node->list);
    INIT_LIST_HEAD(&node->list);//signal to the callback that node was already removed from list
    spin_unlock_irqrestore(&indicators_spinlock, flags);
    if(is_module_currently_unloaded)
    {
        //symply wait while callback will remove indicator
        module_weak_ref_wait();
    }
    else
    {
        //delete by hands
        indicator_descriptor_destroy_waiting(node);
    	kfree(node);
    }
}
EXPORT_SYMBOL(kedr_fsim_indicator_function_unregister);

/*
 * Set indicator function for simulation point.
 *
 * Function behaviour is similar to kedr_fsim_indicator_set,
 * but instead of indicator function it accept name of this indicator
 * (this name should be binding with indicator function via
 * kedr_fsim_indicator_function_register).
 * Also, others parameters of indicator are passed via array of bytes.
 *
 * 'indicator_name' NULL or "" means to clear indicator for the point.
 */

int kedr_fsim_set_indicator_by_name(const char* point_name,
	const char* indicator_name, const void* params, size_t params_len)
{
	void* indicator_state;
	int result = 0;
	struct indicator_descriptor* node;
    unsigned long flags;

	if(indicator_name == NULL || *indicator_name == '\0')
	{
		//clear indicator for the point
		return kedr_fsim_indicator_clear(point_name);
	}
	spin_lock_irqsave(&indicators_spinlock, flags);
	node = indicator_descriptor_lookup(indicator_name);
	if(node == NULL)
    {
        spin_unlock_irqrestore(&indicators_spinlock, flags);
        print_error("No indicator is registered for name %s",
            indicator_name);
        return -1;
    }
	indicator_descriptor_ref(node);
	spin_unlock_irqrestore(&indicators_spinlock, flags);

	if(node->init_state)
	{
		result = node->init_state(params, params_len, &indicator_state);
		if(result)
        {
            print_error0("Function for initialize state failed to create state.");
            indicator_descriptor_unref(node);
            return -2;
        }
	}
	else
	{
		indicator_state = NULL;
	}
	result = kedr_fsim_indicator_set(point_name,
		node->fi, node->format_string,
		node->m, indicator_state, node->destroy_state);
	if(result)
	{
		if(node->destroy_state) node->destroy_state(indicator_state);
	}
    indicator_descriptor_unref(node);
	return result;
}
EXPORT_SYMBOL(kedr_fsim_set_indicator_by_name);

////////////////////////Implementation of auxiliary functions///////////
//
void
indicator_descriptor_init(struct indicator_descriptor* indicator,
    const char* indicator_name,
	kedr_fsim_fault_indicator fi,
	const char* format_string,
	struct module* m,
	kedr_fsim_init_indicator_state init_state,
	kedr_fsim_destroy_indicator_state destroy_state)
{
    indicator->indicator_name = indicator_name;
    indicator->fi = fi;
    indicator->format_string = format_string;
    indicator->m = m;
    indicator->init_state = init_state;
    indicator->destroy_state = destroy_state;

    atomic_set(&indicator->refs, 1);
    init_waitqueue_head(&indicator->wait_finish);
}
//
void
indicator_descriptor_destroy(struct indicator_descriptor* indicator)
{
}
//
void
indicator_descriptor_ref(struct indicator_descriptor* indicator)
{
    atomic_inc(&indicator->refs);
}
//
void
indicator_descriptor_unref(struct indicator_descriptor* indicator)
{
    if(atomic_dec_and_test(&indicator->refs))
    {
        wake_up_interruptible(&indicator->wait_finish);
    }
}
//
static void indicator_descriptor_destroy_waiting(struct indicator_descriptor* indicator)
{
    DEFINE_WAIT(wait);

    prepare_to_wait(&indicator->wait_finish, &wait, TASK_INTERRUPTIBLE);
    indicator_descriptor_unref(indicator);
    schedule();
    finish_wait(&indicator->wait_finish, &wait);
    indicator_descriptor_destroy(indicator);
}
//
struct indicator_descriptor*
indicator_descriptor_lookup(const char* indicator_name)
{
	struct indicator_descriptor* node;
	list_for_each_entry(node, &indicators, list)
	{
		if(strcmp(node->indicator_name, indicator_name) == 0)
			return node;
	}
	return NULL;
}

void on_module_unload(struct module* m,
	struct indicator_descriptor* node)
{
	unsigned long flags;
	//should be revisited
	spin_lock_irqsave(&indicators_spinlock, flags);
    if(!list_empty(&node->list))
	    list_del(&node->list);
    spin_unlock_irqrestore(&indicators_spinlock, flags);
    indicator_descriptor_destroy_waiting(node);
	kfree(node);
}

static __init int fsim_indicator_manager_init(void)
{
	debug0("fault_indicator_manage module starts.");
    module_weak_ref_init();
    spin_lock_init(&indicators_spinlock);
	// Register indicator functions
	kedr_fsim_indicator_function_register("always_fault",
		indicator_always_fault, NULL,
		THIS_MODULE, NULL, NULL);
	kedr_fsim_indicator_function_register("always_fault_after_init",
		indicator_always_fault_after_init, NULL,
		THIS_MODULE, NULL, NULL);
	kedr_fsim_indicator_function_register("fault_if_size_greater",
		(kedr_fsim_fault_indicator)
			indicator_fault_if_size_greater, "size_t",
		THIS_MODULE,
		(kedr_fsim_init_indicator_state)
			indicator_fault_if_size_greater_init_state,
		(kedr_fsim_destroy_indicator_state)
			indicator_fault_if_size_greater_destroy_state);

	// Register kernel part of syscall mechanism
	kedr_fsim_set_indicator_id = sc_register_callback_for_unused_type(
		communicate_set_indicator, NULL, NULL);
	if(kedr_fsim_set_indicator_id == -1)
	{
		print_error0("Failed to register callback for 'set_indicator' interaction.");
		return -1;
	}
    if(sc_library_register(fsim_library_name,
        indicator_manager_library_try_use, indicator_manager_library_unuse,
        &kedr_fsim_set_indicator_id, sizeof(kedr_fsim_set_indicator_id)))
    {
		print_error0("Failed to register fsim library.");
        sc_unregister_callback_for_type(kedr_fsim_set_indicator_id, 1);
    }
	return 0;
}

static __exit void fsim_indicator_manager_exit(void)
{
	sc_unregister_callback_for_type(kedr_fsim_set_indicator_id, 1);

	while(!list_empty(&indicators))
	{
		struct indicator_descriptor* ind = list_entry(
			indicators.next, struct indicator_descriptor, list);
		kedr_fsim_indicator_function_unregister(ind->indicator_name);
	}
    module_weak_ref_destroy();
	debug0("fault_indicator_manage module ends.");
}


static void
communicate_set_indicator(const sc_interaction* interaction,
	const void* buf, size_t len, void* data)
{
    struct kedr_fsim_set_indicator_payload payload_struct = {};
    struct kedr_fsim_set_indicator_reply reply;
    void* payload;
    size_t payload_length;
	(void)data; //unused

    if(kedr_fsim_set_indicator_payload_get(&payload_struct,
		buf, len))
	{
		print_error0("Incorrect format of the message.");
		return;
	}

    debug("Set indicator \"%s\" for point \"%s\".",
		payload_struct.indicator_name,
		payload_struct.point_name);

    reply.result = kedr_fsim_set_indicator_by_name(
		payload_struct.point_name,
		payload_struct.indicator_name,
		payload_struct.params, payload_struct.params_len);

	payload_length = kedr_fsim_set_indicator_reply_len(&reply);
	payload = kmalloc(payload_length, GFP_KERNEL);

	kedr_fsim_set_indicator_reply_put(&reply, payload);

	sc_send(interaction, payload, payload_length);
	kfree(payload);

}

int indicator_manager_library_try_use()
{
    return try_module_get(THIS_MODULE) == 0;
}
void indicator_manager_library_unuse()
{
    module_put(THIS_MODULE);
}


module_init(fsim_indicator_manager_init);
module_exit(fsim_indicator_manager_exit);
