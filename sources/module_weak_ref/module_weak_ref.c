#include <kedr/module_weak_ref/module_weak_ref.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h> /* mutexes */
#include <linux/spinlock.h> /* spinlocks */
/*
 * List of 'destroy' functions(and their user_data) for some module.
 */

struct destroy_data
{
	struct list_head list;
	destroy_notify destroy;
	void* user_data;
    /*
     * currently_called=0, if one can safetly remove this node and its data(under spinlock locked),
     * otherwise 1.
     * (when 1, make this node itseld, and 'list', 'destroy' and 'user_data' fields readonly for others even without spinlock taken.)
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
     * currently_unloaded=0, if one can safetly remove this node (under spinlock locked),
     * otherwise 1.
     * (when 1, make this node itself and 'list' member readonly for others even without spinlock taken.)
     */
    int currently_unloaded;
};

// Head of the list of modules
static struct list_head module_weak_ref_list;

/* 
 * Spinlock which protect(against concurrent read and write):
 * -list of modules,  except node for unloading one
 * -list of destroy functions for each module, except node for currently executed callback
 */

static spinlock_t module_weak_ref_spinlock;

/*
 * Mutex for wait current callback execution.
 */

static struct mutex module_weak_ref_callback_mutex;

// Indicator, whether all functionality is currently initialized.
static int is_initialized = 0;

// Auxiliary functions

/* 
 * Look for node for given module.
 * If found, return this node. Otherwise, return NULL.
 *
 * Should be called with module_weak_ref_spinlock taken.
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


// Should be called for use weak reference functionality on module.
int module_weak_ref_init(void)
{
	int result;
	
	if(is_initialized) return 1; //already initialized
    
	spin_lock_init(&module_weak_ref_spinlock);
    mutex_init(&module_weak_ref_callback_mutex);
    INIT_LIST_HEAD(&module_weak_ref_list);
    
	result = register_module_notifier(&detector_nb);
    if(result)
    {
        mutex_destroy(&module_weak_ref_callback_mutex);
        return result;//fail to initialized
    }
    
    is_initialized = 1;
    return 0;
}
// Should be called when the functionality will no longer be used.
void module_weak_ref_destroy(void)
{
    if(is_initialized) return;//already destroyed
    
	BUG_ON(!list_empty(&module_weak_ref_list));
	unregister_module_notifier(&detector_nb);
    mutex_destroy(&module_weak_ref_callback_mutex);
    
    is_initialized = 0;
}

// Shedule 'destroy' function to call when module is unloaded.
void module_weak_ref(struct module* m,
	destroy_notify destroy, void* user_data)
{
	struct destroy_data* ddata;
    struct module_weak_ref_data *mdata, *mdata_new;
	unsigned long flags;
    
	if(!is_initialized) return;//silently returned
	
	ddata = kmalloc(sizeof(*ddata), GFP_KERNEL);
	ddata->destroy = destroy;
	ddata->user_data = user_data;
    ddata->currently_called = 0;
    
    spin_lock_irqsave(&module_weak_ref_spinlock, flags);
	mdata = get_module_node(m);
	// Add new node only if node for module already exist
	if(mdata != NULL)
    {
    	list_add_tail(&ddata->list, &mdata->destroy_data);
    }
    spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
    
	if(mdata != NULL) return;
    // Otherwise create node for module before, but without spinlock taken.
	mdata_new = kmalloc(sizeof(*mdata_new), GFP_KERNEL);
	mdata_new->m = m;
    mdata_new->currently_unloaded = 0;
	INIT_LIST_HEAD(&mdata_new->destroy_data);
    spin_lock_irqsave(&module_weak_ref_spinlock, flags);
    // Look for node for module again, because we was released spinlock.
	mdata = get_module_node(m);
	if(mdata == NULL)
    {
    	list_add_tail(&mdata_new->list, &module_weak_ref_list);
        mdata = mdata_new;
    }
    else //someone add node for module while we prepare ourself one
    {
        kfree(mdata_new);
    }
    list_add_tail(&ddata->list, &mdata->destroy_data);
    spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
}
// Cancel sheduling.
int module_weak_unref(struct module* m,
	destroy_notify destroy, void* user_data)
{
	int result = -1;
	struct destroy_data* ddata;
    struct module_weak_ref_data* mdata;
    unsigned long flags;

	spin_lock_irqsave(&module_weak_ref_spinlock, flags);
	mdata = get_module_node(m);
	
    if(mdata != NULL)
    {
    	list_for_each_entry(ddata, &mdata->destroy_data, list)
    	{
    		if(ddata->destroy == destroy && ddata->user_data == user_data)
    		{
    			result = ddata->currently_called;
    			if(!result)
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
    spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
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
    unsigned long flags;
	//If module is not unloading do nothing
	if(mod_state != MODULE_STATE_GOING) return 0;
    
    spin_lock_irqsave(&module_weak_ref_spinlock, flags);
	mdata = get_module_node(vmod);
    mdata->currently_unloaded = 1; //prevent mdata to be invalidated
    spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
    
    BUG_ON(mdata == NULL);
	//without mutex lock, but it shouldn't be invalidated because of 'currently_unloaded' flag
	if(mdata != NULL)
	{
    	spin_lock_irqsave(&module_weak_ref_spinlock, flags);
    	while(!list_empty(&mdata->list))
        {
            ddata = list_first_entry(&mdata->destroy_data, struct destroy_data, list);
            ddata->currently_called = 1; //prevent ddata to be invalidated
            /*
             * Take mutex while spinlock is taken.
             *
             *(!!!!)We take new lock while holding another, 
             * because otherwise module_weak_ref_wait() may be executed and returned
             * before we take mutex for callback, and callback will be executed after it,
             * that is error.
             */
            mutex_lock(&module_weak_ref_callback_mutex);
            spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
            
            ddata->destroy(vmod, ddata->user_data);
            mutex_unlock(&module_weak_ref_callback_mutex);
            //reaquire spinlock
    	    spin_lock_irqsave(&module_weak_ref_spinlock, flags);
            list_del(&ddata->list);
    		kfree(ddata);
        }
    	list_del(&mdata->list);
    	kfree(mdata);
        spin_unlock_irqrestore(&module_weak_ref_spinlock, flags);
    }
	return 0;
}

void module_weak_ref_wait()
{
    //simple take and release mutex for callback
    mutex_lock(&module_weak_ref_callback_mutex);
    mutex_unlock(&module_weak_ref_callback_mutex);
}