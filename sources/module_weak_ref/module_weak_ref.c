#include <kedr/module_weak_ref/module_weak_ref.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h> /* mutexes */
/*
 * List of 'destroy' functions(and their user_data) for some module.
 */

struct destroy_data
{
	struct list_head list;
	destroy_notify destroy;
	void* user_data;
    /*
     * currently_called=0, if one can safetly remove this node and its data(under mutex locked),
     * otherwise 1.
     * (make node, except 'list' member, readonly for others)
     */
    int currently_called;
};

/*
 * List of modules, for which 'destroy' functions are currently registered.
 */

struct module_weak_ref_data
{
	struct list_head list;
	struct module* m;
	struct list_head destroy_data;
    /*
     * currently_unloaded=0, if one can safetly remove this node (under mutex locked),
     * otherwise 1.
     */
    int currently_unloaded;
};

// pointer to list of modules
LIST_HEAD(module_weak_ref_list);
/* 
 * Mutex which protect(against concurrent read and write):
 * -list of modules,  except node for unloading one
 * -list of destroy functions for each module, except node for currently executed callback
 */
DEFINE_MUTEX(module_weak_ref_mutex);
/*
 * Mutex for wait current callback execution.
 */
DEFINE_MUTEX(module_weak_ref_callback_mutex);
// Auxiliary functions
/* 
 * Look for node for given module.
 * If found, return this node. Otherwise, return NULL.
 *
 * [!] Should be called under module_weak_ref_mutex locked.
 */

static struct module_weak_ref_data* get_module_node(struct module* m);

/*
 * Called when some module is unloading.
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


// Should be called for use weak reference functionality on module.
int module_weak_ref_init(void)
{
	return register_module_notifier(&detector_nb);
}
// Should be called when the functionality will no longer be used.
void module_weak_ref_destroy(void)
{
	BUG_ON(!list_empty(&module_weak_ref_list));
	unregister_module_notifier(&detector_nb);
}
// Shedule 'destroy' function to call when module is unloaded.
void module_weak_ref(struct module* m,
	destroy_notify destroy, void* user_data)
{
	struct destroy_data* ddata;
    struct module_weak_ref_data* mdata;
	
	ddata = kmalloc(sizeof(*ddata), GFP_KERNEL);
	ddata->destroy = destroy;
	ddata->user_data = user_data;
    ddata->currently_called = 0;

    while(mutex_lock_interruptible(&module_weak_ref_mutex));
	mdata = get_module_node(m);
	if(mdata == NULL)
	{
		mdata = kmalloc(sizeof(*mdata), GFP_KERNEL);
		mdata->m = m;
        mdata->currently_unloaded = 0;
		INIT_LIST_HEAD(&mdata->destroy_data);
		list_add_tail(&mdata->list, &module_weak_ref_list);
	}
	list_add_tail(&ddata->list, &mdata->destroy_data);
    mutex_unlock(&module_weak_ref_mutex);
}
// Cancel sheduling.
int module_weak_unref(struct module* m,
	destroy_notify destroy, void* user_data)
{
	int result = -1;
	struct destroy_data* ddata;
    struct module_weak_ref_data* mdata;
    while(mutex_lock_interruptible(&module_weak_ref_mutex));
	
	mdata = get_module_node(m);
	
    if(mdata != NULL)
    {
    	list_for_each_entry(ddata, &mdata->destroy_data, list)
    	{
    		if(ddata->destroy == destroy && ddata->user_data == user_data)
    		{
    			result = ddata->currently_called;
    			if(result)
                {
        			list_del(&ddata->list);
        			kfree(ddata);
                }
    			break;
    		}
    	}
    	if(list_empty(&mdata->destroy_data) && !mdata->currently_unloaded)
    	{
    		list_del(&mdata->list);
    		kfree(mdata);
    	}
    }
    mutex_unlock(&module_weak_ref_mutex);
    BUG_ON(mdata == NULL);//module not found
    BUG_ON(result == -1);//callback not found
    return result;
}

static struct module_weak_ref_data* get_module_node(struct module* m)
{
	struct module_weak_ref_data* node;
	list_for_each_entry(node, &module_weak_ref_list, list)
	{
		if(node->m == m) return node;
	}
	return NULL;
}

static int 
detector_modules_unload(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module_weak_ref_data* mdata;
	struct destroy_data* ddata;
	//If module is not unloading do nothing
	if(mod_state != MODULE_STATE_GOING) return 0;
    while(mutex_lock_interruptible(&module_weak_ref_mutex));
	mdata = get_module_node(vmod);
    mdata->currently_unloaded = 1; //prevent mdata to be invalidated
    mutex_unlock(&module_weak_ref_mutex);
    BUG_ON(mdata == NULL);
	//without mutex lock, but it shouldn't be invalidated because of 'currently_unloaded' flag
	if(mdata != NULL)
	{
    	while(mutex_lock_interruptible(&module_weak_ref_mutex));
    	while(!list_empty(&mdata->list))
        {
            ddata = list_first_entry(&mdata->destroy_data, struct destroy_data, list);
            ddata->currently_called = 1; //prevent ddata to be invalidated
            /*
             * Take another mutex - while callback is executed.
             *
             *(!!!!)We take new mutex while holding another, 
             * because otherwise module_weak_ref_wait() may be executed and returned
             * before we take mutex for callback, and callback will be executed after it,
             * that is error.
             */
            while(mutex_lock_interruptible(&module_weak_ref_callback_mutex));
            mutex_unlock(&module_weak_ref_mutex);
            
            
            ddata->destroy(vmod, ddata->user_data);
            mutex_unlock(&module_weak_ref_callback_mutex);
            //reaquire old mutex
            while(mutex_lock_interruptible(&module_weak_ref_mutex));
            list_del(&ddata->list);
    		kfree(ddata);
        }
    	list_del(&mdata->list);
    	kfree(mdata);
        mutex_unlock(&module_weak_ref_mutex);
    }
	return 0;
}

void module_weak_ref_wait()
{
    //simple take and release mutex for callback
    while(mutex_lock_interruptible(&module_weak_ref_callback_mutex));
    mutex_unlock(&module_weak_ref_callback_mutex);
}