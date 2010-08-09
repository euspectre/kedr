/*
 * Implements API for managing indicators functions.
 * 
 * Also, implement socket for manipulating with this indicators
 * from user space.
 */

#include <kedr/fault_simulation/fsim_indicator_manager.h>
#include <kedr/fault_simulation/fsim_indicator_manager_internal.h> /* socket protocol for API*/

#include <linux/module.h>
#include <linux/list.h> /* list functions */
#include <linux/slab.h> /* kmalloc & kfree */

/* weak reference of module functionality */
#include <kedr/module_weak_ref/module_weak_ref.h>

/* kedr_target_module_in_init */
#include <kedr/base/common.h>
/* kernel-user spaces interaction */
#include <kedr/syscall_connector/syscall_connector.h>

#define debug(str, ...) printk(KERN_DEBUG "%s: " str, __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str, __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

struct indicator_descriptor
{
	struct list_head list;
	
	const char* indicator_name;
	kedr_fsim_fault_indicator fi;
	const char* format_string;
	struct module* m;
	kedr_fsim_init_indicator_state init_state;
	kedr_fsim_destroy_indicator_state destroy_state;
};

static struct list_head indicators = LIST_HEAD_INIT(indicators);

// Auxiliary functions

// find existing descriptor by name
static struct indicator_descriptor* 
indicator_descriptor_lookup(const char* indicator_name);

/*
 * Callback function, called when module which 
 * registers indicator, is unloaded.
 */

static void on_module_unload(struct module* m, 
	struct indicator_descriptor* node)
{
	list_del(&node->list);
	kfree(node);
}
// Functions for interact with user space
static void 
communicate_set_indicator(sc_interaction* interaction,
	const void* buf, size_t len, void* data);
static void 
communicate_init(sc_interaction* interaction,
	const void* buf, size_t len, void* data);
static void 
communicate_break(sc_interaction* interaction,
	const void* buf, size_t len, void* data);

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
		print_error("Size of payload %zu, expected size %zu.\n",
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
	if(*indicator_name == '\0')
	{
		print_error0("Indicator name \"\" is reserved for "
			"special use - clearing indicator.\n");
		return -1;
	}
	if(indicator_descriptor_lookup(indicator_name)) return -1;
	
	new_node = kmalloc(sizeof(*new_node), GFP_KERNEL);
	if(new_node == NULL)
	{
		print_error0("Cannot allocate new indicator descriptor.\n");
		return -2;
	}
	new_node->indicator_name = indicator_name;
	new_node->fi = fi;
	new_node->format_string = format_string;
	new_node->m = m;
	new_node->init_state = init_state;
	new_node->destroy_state = destroy_state;
	
	list_add_tail(&new_node->list, &indicators);
	module_weak_ref(m, (destroy_notify)on_module_unload, new_node);
	
	return 0;
}
EXPORT_SYMBOL(kedr_fsim_indicator_function_register);

/*
 * Unregister indicator function.
 */

void kedr_fsim_indicator_function_unregister(const char* indicator_name)
{
	struct indicator_descriptor* node =
		indicator_descriptor_lookup(indicator_name);
	if(node == NULL) return;
	
	module_weak_unref(node->m, (destroy_notify)on_module_unload, node);
	
	list_del(&node->list);
	kfree(node);
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
	if(indicator_name == NULL || *indicator_name == '\0')
	{
		//clear indicator for the point
		return kedr_fsim_indicator_clear(point_name);
	}
	node = indicator_descriptor_lookup(indicator_name);
	if(node == NULL) return -1;
	
	if(node->init_state)
	{
		result = node->init_state(params, params_len, &indicator_state);
		if(result) return -2;
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
	return result;
}
EXPORT_SYMBOL(kedr_fsim_set_indicator_by_name);

static struct indicator_descriptor* 
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

static __init int fault_indicator_manage_init(void)
{
	debug0("fault_indicator_manage module starts.\n");
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
	if(sc_register_callback_for_type(kedr_fsim_init_id,
		communicate_init, NULL))
	{
		print_error0("Interaction type of initialization "
			"communication already in use.\n");
		return -1;
	}
	if(sc_register_callback_for_type(kedr_fsim_set_indicator_id,
		communicate_set_indicator, NULL))
	{
		print_error0("Interaction type of set indicator "
			"communication already in use.\n");
		return -1;

	}
	if(sc_register_callback_for_type(kedr_fsim_break_id,
		communicate_break, NULL))
	{
		print_error0("Interaction type of finalization "
			"communication already in use.\n");
		return -1;
	}
	return 0;
}

static __exit void fault_indicator_manage_exit(void)
{
	sc_unregister_callback_for_type(kedr_fsim_init_id);
	sc_unregister_callback_for_type(kedr_fsim_set_indicator_id);
	sc_unregister_callback_for_type(kedr_fsim_break_id);
	
	while(!list_empty(&indicators))
	{
		struct indicator_descriptor* ind = list_entry(
			indicators.next, struct indicator_descriptor, list);
		kedr_fsim_indicator_function_unregister(ind->indicator_name);
	}		
	
	debug0("fault_indicator_manage module ends.\n");
}


static void 
communicate_set_indicator(sc_interaction* interaction,
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
		print_error0("Incorrect format of the message.\n");
		return;
	}
    
    debug("Set indicator \"%s\" for point \"%s\".\n",
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


static void communicate_init(sc_interaction* interaction,
	const void* buf, size_t len, void* data)
{
	debug0("init communication starts.\n");
	
	(void) data;
	(void) buf;//may be asserted in the future
	(void) len;//may be asserted in the future
	
	try_module_get(THIS_MODULE);
	sc_send(interaction, NULL, 0);

	debug0("init communication ends.\n");
}

static void communicate_break(sc_interaction* interaction,
	const void* buf, size_t len, void* data)
{
	(void) interaction;
	(void) data;
	(void) buf;
	(void) len;

	debug0("break communication starts.\n");
	module_put(THIS_MODULE);
	debug0("break communication ends.\n");
}
MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");


module_init(fault_indicator_manage_init);
module_exit(fault_indicator_manage_exit);