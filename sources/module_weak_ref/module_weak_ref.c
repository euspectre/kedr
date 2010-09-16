/*
 * Implementation of module_weak_ref functionality.
 */

#include <kedr/module_weak_ref/module_weak_ref.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h> /* spinlocks */
#include <linux/wait.h> /* for wait deleting of indicator */
#include <linux/sched.h> /* schedule */


#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)


/*
 * List of 'destroy' functions(and their user_data) for some module.
 */

struct destroy_data
{
	struct list_head list;
	destroy_notify destroy;
	void* user_data;
    /*
     * If not 0, prevent this node from free.
     *
     * Set to 1 before call to 'destroy' function. After call, this node will be removed from list(if needed, under spinlock)
     * and freed.
     */
    int currently_called;
};

//initialize and destroy function for structure
static void destroy_data_init(struct destroy_data *ddata,
    destroy_notify destroy,	void* user_data);
static void destroy_data_destroy(struct destroy_data *ddata);

/*
 * List of modules, for which 'destroy' functions are currently registered.
 */

struct module_weak_ref_data
{
	struct list_head list;
	struct module* m;
	struct list_head destroy_data;
    /*
     * If not 0, prevent this node from free.
     *
     * Set to 1 when module go to unload. After call of all callback functions, this node will be removed from list(if needed, under spinlock)
     * and freed.
     */
    int currently_unloaded;
};
//initialize and destroy function for structure
static void module_weak_ref_data_init(struct module_weak_ref_data *mdata, struct module *m);
static void module_weak_ref_data_destroy(struct module_weak_ref_data *mdata);
// Head of the list of modules
static LIST_HEAD(module_weak_ref_list);

/*
 * Spinlock which protect(against concurrent read and write):
 * -list of modules
 * -list of destroy functions for each module
 */

static DEFINE_SPINLOCK(module_weak_ref_spinlock);

/*
 * Waitqueue for wait in module_weak_ref_wait().
 */

static DECLARE_WAIT_QUEUE_HEAD(wait_callback);

/*
 * For module_weak_ref_wait() implementation.
 *
 * This variable is incremented every time, when callback is called or finished.
 * So, odd value means that callback is currently executed, even value - that not.
 *
 * For use as wait expression, this value is not protected with anly lock.
 * It increment wraps out 'currently_called=1' of 'struct destroy_data' state.
 *
 * Precisely, only that is ensured:
 * -if (protected) read of 'currently_called' return 1 for some 'struct destroy_data' node,
 *  and consequent read of this variable return even value, then that 'struct destroy_data' node
 *  was finished to execute.
 * -if this variable has odd value, than in the future it will have even value, after (possible)
 * executing callback.
 *
 * NOTE: atomic_inc() and atomic_get() calls are ordered ONLY TO THEMSELVES. Them DO NOT IMPLY
 * order with instructions of other variables. So, use barrier mechanisms.
 *
 */

static atomic_t callback_generation = ATOMIC_INIT(0);

// Indicator, whether all functionality is currently initialized.
static int is_initialized = 0;

// Auxiliary functions

/*
 * Look for node for given module.
 * If found, return this node. Otherwise, return NULL.
 *
 * Should be called under spinlock taken.
 */

static struct module_weak_ref_data* get_module_node(struct module* m);

/*
 * Callback, called when some module change its state(for detect unloading).
 */

static int
detector_modules_unload(struct notifier_block *nb,
	unsigned long mod_state, void *vmod);

// struct for watching for loading/unloading of modules.
struct notifier_block detector_nb = {
	.notifier_call = detector_modules_unload,
	.next = NULL,
	.priority = 3, /*Some number*/
};
///////////////////Implementation of exported functions/////////////////////

// Should be called for use weak reference functionality on module.
int module_weak_ref_init(void)
{
    BUG_ON(is_initialized);

	if(register_module_notifier(&detector_nb)) return 1;//fail to initialize

    is_initialized = 1;
    debug0("module_weak_ref functionality is initialized.");
    return 0;
}
// Should be called when the functionality will no longer be used.
void module_weak_ref_destroy(void)
{
    BUG_ON(!is_initialized);
	BUG_ON(!list_empty(&module_weak_ref_list));

	WARN_ON(unregister_module_notifier(&detector_nb));

    is_initialized = 0;
    debug0("module_weak_ref functionality is destroyed.");
}

// Shedule 'destroy' function to call when module is unloaded.
void module_weak_ref(struct module* m,
	destroy_notify destroy, void* user_data)
{
	struct destroy_data* ddata;
    struct module_weak_ref_data *mdata;
	unsigned long flags;

	BUG_ON(!is_initialized);

	ddata = kmalloc(sizeof(*ddata), GFP_KERNEL);
    if(ddata == NULL)
    {
        print_error0("Cannot allocate memory for 'struct destroy_data'.");
        return;
    }
    destroy_data_init(ddata, destroy, user_data);

    spin_lock_irqsave(&module_weak_ref_spinlock, flags);
	mdata = get_module_node(m);

	if(mdata == NULL)
    {
        //create new node for given module
        struct module_weak_ref_data *mdata_new;
        //cannot call kmalloc under spinlock taken
        spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
        debug0("Create new node for module");
        mdata_new = kmalloc(sizeof(*mdata_new), GFP_KERNEL);
        if(mdata_new == NULL)
        {
            print_error0("Cannot allocate memory for 'struct module_weak_ref_data'.");
            return;
        }
        module_weak_ref_data_init(mdata_new, m);
        //again aquire spinlock, for insert node for new module
        spin_lock_irqsave(&module_weak_ref_spinlock, flags);
        //..but need to repeat search
        mdata = get_module_node(m);
        if(mdata == NULL)
        {
            //insert our node
            list_add_tail(&mdata_new->list, &module_weak_ref_list);
            mdata = mdata_new;
        }
        else
        {
            //someone insert new node while we create it
            module_weak_ref_data_destroy(mdata_new);
            kfree(mdata_new);
        }
    }
    list_add_tail(&ddata->list, &mdata->destroy_data);
    spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
    debug0("Add callback for module");
}
// Cancel sheduling.
int module_weak_unref(struct module* m,
	destroy_notify destroy, void* user_data)
{
	int result = -1;
	struct destroy_data* ddata;
    struct module_weak_ref_data* mdata;
    unsigned long flags;

    BUG_ON(!is_initialized);

	spin_lock_irqsave(&module_weak_ref_spinlock, flags);
	mdata = get_module_node(m);

    if(mdata == NULL)
    {
        //Function was called with incorrect parameter - module
        spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
        __WARN_printf("Unexistent record for unref - perhaps, callback was already finished and you have race.");
        return 0;//as if scheduling was canceled
    }
	list_for_each_entry(ddata, &mdata->destroy_data, list)
	{
		if((ddata->destroy == destroy) && (ddata->user_data == user_data))
		{
			list_del(&ddata->list);
            INIT_LIST_HEAD(&ddata->list);
			if(!ddata->currently_called)
            {
                result = 0;
                destroy_data_destroy(ddata);
    			kfree(ddata);
            }
            else result = 1;
			break;
		}
	}
    if(result == -1)
    {
        //Function was called with incorrect parameter - callback
        spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
        __WARN_printf("Unexistent record for unref - perhaps, callback was already finished and you have race.");
        return 0;//as if scheduling was canceled
    }

	if(list_empty(&mdata->destroy_data))
	{
		//free unused node for module
		list_del(&mdata->list);
        INIT_LIST_HEAD(&mdata->list);
        if(!mdata->currently_unloaded)
        {
            module_weak_ref_data_destroy(mdata);
		    kfree(mdata);
        }
	}
    spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
    return result;
}

void module_weak_ref_wait()
{
    int current_callback_generation = atomic_read(&callback_generation);

    BUG_ON(!is_initialized);

    if(current_callback_generation & 0x1)
    {
        DEFINE_WAIT(wait);
        prepare_to_wait(&wait_callback, &wait, TASK_INTERRUPTIBLE);

        while(atomic_read(&callback_generation) == current_callback_generation)
            schedule();
        finish_wait(&wait_callback, &wait);
    }
}

////////////////Implementation of auxiliary function//////////////////////
//
void destroy_data_init(struct destroy_data *ddata,
    destroy_notify destroy,	void* user_data)
{
    ddata->destroy = destroy;
    ddata->user_data = user_data;
    ddata->currently_called = 0;
}
//
void destroy_data_destroy(struct destroy_data *ddata)
{
}
//
void module_weak_ref_data_init(struct module_weak_ref_data *mdata, struct module *m)
{
    mdata->m = m;
    mdata->currently_unloaded = 0;
	INIT_LIST_HEAD(&mdata->destroy_data);
}
//
void module_weak_ref_data_destroy(struct module_weak_ref_data *mdata)
{
}


struct module_weak_ref_data* get_module_node(struct module* m)
{
	struct module_weak_ref_data* node;
	list_for_each_entry(node, &module_weak_ref_list, list)
	{
		if(node->m == m) return node;
	}
	return NULL;
}

int
detector_modules_unload(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{

	struct module_weak_ref_data* mdata;

    unsigned long flags;
	debug0("Found module which change state.");
    //If module is not unloading do nothing
	if(mod_state != MODULE_STATE_GOING) return 0;
    debug0("Found unloaded module");
    spin_lock_irqsave(&module_weak_ref_spinlock, flags);
	mdata = get_module_node(vmod);//module may not be found, because we catch changing state of ALL modules
    if(mdata == NULL)
    {
        //We doesn't interested in that module
        spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
        return 0;
    }
    mdata->currently_unloaded = 1; //prevent mdata to be invalidated when we release spinlock
	while(!list_empty(&mdata->destroy_data))
    {
        struct destroy_data* ddata = list_first_entry(&mdata->destroy_data, struct destroy_data, list);
        ddata->currently_called = 1; //prevent ddata to be invalidated when we release spinlock
        //spinlock do not protect 'callback_generation', because it is read without it.
        //but it sinchronize data
        atomic_inc(&callback_generation);
        spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
        debug0("Calling callback...");

        ddata->destroy(vmod, ddata->user_data);

        debug0("...callback has finished.");

        //barrier - 'destroy' is known as fully finished.
        smp_mb__before_atomic_inc();
        atomic_inc(&callback_generation);
        //wake_up_interruptible is barrier itself
        wake_up_interruptible(&wait_callback);

        //reaquire spinlock for delete node
	    spin_lock_irqsave(&module_weak_ref_spinlock, flags);

        if(!list_empty(&ddata->list))
            list_del(&ddata->list);
		destroy_data_destroy(ddata);
		kfree(ddata);
    }
	if(!list_empty(&mdata->list))
	    list_del(&mdata->list);
    spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
    debug0("All callbacks finished for module.");
    module_weak_ref_data_destroy(mdata);
	kfree(mdata);
    debug0("return.");
	return 0;
}
