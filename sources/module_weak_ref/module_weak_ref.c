/*
 * Implementation of module_weak_ref functionality.
 */

#include <kedr/module_weak_ref/module_weak_ref.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h> /* spinlocks */

#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

/*
 * From concept to implementation:
 *
 * 0. 'struct module m' = 'wobj_t obj'.//It is not true
 *
 * 1. 'struct module m' has 'wobj_t obj' field.//Even this is not true
 *
 * 2. 'struct module m' is mapped to 'wobj_t obj'.//This is may be implemented, but 'obj' without weak references to it is memory leak.
 * 
 */


//Image of kernel module as object
struct module_image
{
    struct list_head list;//map=list
    struct module* m;//identificator of the node
    
    wobj_t obj;//'main' object
};

/*
 * List of images for all modules.
 *
 * -if obj is not existed for module, which is going to unload, then no weak reference was used for this module.
 * -if obj is existed for some module, then this node may be deleted only when module go to unload, or when 'module_weak_ref' is destroyed.
 * 
 * NOTE: it is not a error for list to be not empty when module_weak_ref_destroy() is called.
 * Error is for some object in the list to have callback function from weak references, but it cannot be verified
 * (in the current implementation architecture).
 */

static LIST_HEAD(module_image_list);

// Spinlock which protect list
static DEFINE_SPINLOCK(module_image_spinlock);

static void module_image_init(struct module_image* mi, struct module* m);
static void module_image_destroy(struct module_image* mi);

/*
 * Look for node for given module.
 * If found, return this node. Otherwise, return NULL.
 *
 * Should be called under spinlock taken.
 */

static struct module_image* get_module_image(struct module* m);

// Indicator, whether all functionality is currently initialized.
static int is_initialized = 0;

// Auxiliary functions

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
/*
 *  Its function may be called only after it is known, that all weak references to the module obj are cleared.
 *  But it is difficult to determine, whether all module object has no callbacks.
 *
 *  We do not call this callbacks. May be, that will generate bug situation in some module. So it reveal problem in logic.
 */
    unsigned long flags;
    struct module_image* mi;
    


    BUG_ON(!is_initialized);

	WARN_ON(unregister_module_notifier(&detector_nb));
    spin_lock_irqsave(&module_image_spinlock, flags);
    while(!list_empty(&module_image_list))
    {
        mi = list_first_entry(&module_image_list, struct module_image, list);
        //do not unref object!!
        module_image_destroy(mi);
        kfree(mi);
    }
    spin_unlock_irqrestore(&module_image_spinlock, flags);
    is_initialized = 0;
    debug0("module_weak_ref functionality is destroyed.");
}

/*
 * May be safetly used in atomic context, because callback functions
 * of weak references will be done in the process, which unloads module.
 *
 * wmodule_unref() is intended to use after successfull wmodule_weak_ref_get
 * (wmodule_ref() is disabled).
 */

void wmodule_unref(struct module* m)
{
    unsigned long flags;
    struct module_image* mi;
    spin_lock_irqsave(&module_image_spinlock, flags);
    mi = get_module_image(m);
    BUG_ON(mi == NULL);//it is bug to unref those, which you not ref'ed.
    wobj_unref(&mi->obj);
    spin_unlock_irqrestore(&module_image_spinlock, flags);
}

void wmodule_weak_ref_init(wmodule_weak_ref_t* wmodule_weak_ref, struct module *m,
    void (*destroy_weak_ref)(wmodule_weak_ref_t* wmodule_weak_ref))
{
    unsigned long flags;
    struct module_image* mi;
    spin_lock_irqsave(&module_image_spinlock, flags);
    mi = get_module_image(m);
    if(mi == NULL)
    {
        struct module_image* mi_new;
        //create node for module
        spin_unlock_irqrestore(&module_image_spinlock, flags);
        mi_new = kmalloc(sizeof(*mi_new), GFP_KERNEL);
        if(mi_new == NULL)
        {
            print_error0("Cannot allocate node for new node for module");
            BUG();//because we cannot report to the user about this, we should report bug.
        }
        module_image_init(mi_new, m);
        spin_lock_irqsave(&module_image_spinlock, flags);
        //need to repeat searching
        mi = get_module_image(m);
        if(mi != NULL)
        {
            //someone register new node, while we create it
            module_image_destroy(mi_new);
            kfree(mi_new);
        }
        else
        {
            list_add(&mi_new->list, &module_image_list);
            mi = mi_new;
        }
    }
    wobj_weak_ref_init(wmodule_weak_ref, &mi->obj, destroy_weak_ref);
    spin_unlock_irqrestore(&module_image_spinlock, flags);
}

struct module* wmodule_weak_ref_get(wmodule_weak_ref_t* wmodule_weak_ref)
{
    struct module* m = NULL;
    wobj_t* obj = wobj_weak_ref_get(wmodule_weak_ref);
    if(obj != NULL)
        m = container_of(obj, struct module_image, obj)->m;
    return m;
}

void wmodule_weak_ref_clear(wmodule_weak_ref_t* wmodule_weak_ref)
{
    wobj_weak_ref_clear(wmodule_weak_ref);
}

void wmodule_wait_callback_prepare(wmodule_wait_callback_t* wait_callback, wmodule_weak_ref_t* wmodule_weak_ref)
{
    wobj_wait_callback_prepare(wait_callback, wmodule_weak_ref);
}
void wmodule_wait_callback_wait(wmodule_wait_callback_t* wait_callback)
{
    wobj_wait_callback_wait(wait_callback);
}



////////////////Implementation of auxiliary function//////////////////////
//
void module_image_init(struct module_image* mi, struct module* m)
{
    mi->m = m;
    wobj_init(&mi->obj, NULL);
}
void module_image_destroy(struct module_image* mi)
{
}

/*
 * Look for node for given module.
 * If found, return this node. Otherwise, return NULL.
 *
 * Should be called under spinlock taken.
 */

struct module_image* get_module_image(struct module* m)
{
  	struct module_image* mi;
	list_for_each_entry(mi, &module_image_list, list)
	{
		if(mi->m == m) return mi;
	}
	return NULL;
}


int
detector_modules_unload(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
    struct module_image* mi;
    unsigned long flags;
	debug0("Found module which change state.");
    //If module is not unloading do nothing
	if(mod_state != MODULE_STATE_GOING) return 0;
    debug0("Found unloaded module");
    spin_lock_irqsave(&module_image_spinlock, flags);
	mi = get_module_image(vmod);//module may not be found, because we catch changing state of ALL modules
    if(mi == NULL)
    {
        //We doesn't interested in that module
        spin_unlock_irqrestore(&module_image_spinlock, flags);
        return 0;
    }
    list_del(&mi->list);
    spin_unlock_irqrestore(&module_image_spinlock, flags);
    wobj_unref_final(&mi->obj);
    debug0("All callbacks finished for module.");
    module_image_destroy(mi);
    kfree(mi);
    debug0("return.");
	return 0;
}
