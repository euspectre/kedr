#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/list.h>		/* list functions */

#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <kedr/module_weak_ref/module_weak_ref.h>
#include <kedr/wobject/wobject.h>
#include <kedr/fault_simulation/fsim_base.h>

#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

/*
 * Indicator is represented by wobject, and the only strong reference to it is contatined in it's weak reference to the module.
 * If module is NULL, the only strong reference to the indicator is contained in it's weak reference to the point(may be implemented in the future).
 *
 */


/*
 * Information about indicator, that needed for correct work of simulate
 */

struct fault_indicator_info
{
	wobj_t obj;//'main' obj
    //function which called at point's simulation stage
	kedr_fsim_fault_indicator fi;
    //current state of the indicator(may be used by indicator function)
	void* indicator_state;
    //function to be called when module 'm' is unloaded
	kedr_fsim_destroy_indicator_state destroy;

    wmodule_weak_ref_t wrm;//weak reference to the module
};

static void fault_indicator_info_init(struct fault_indicator_info* fii,
    kedr_fsim_fault_indicator fi,
    void* indicator_state,
    kedr_fsim_destroy_indicator_state destroy,
    struct module* m);

static void fault_indicator_info_destroy(struct fault_indicator_info* fii);

static void fault_indicator_info_finalize(wobj_t* obj);
//callback
static void fault_indicator_info_on_module_unload(wmodule_weak_ref_t* wrm);

/*
 * Do not wait, while module will unloaded, and delete indicator at this step.
 *
 * May call shedule() (via wobj_unref_final).
 *
 * For convinience, take object of the indicator( its 'obj' field)
 *
 * Note: you should have your own reference to this indicator(e.g., via point->wri).
 */
static void fault_indicator_info_delete_now(wobj_t* indicator_object);
/*
 * Simulation point
 */
 
struct kedr_simulation_point
{
	struct list_head list;
	//name of the point
	const char* name;
    //
    wobj_t obj;//'main' object
    //
    wobj_weak_ref_t wri;//weak reference to the indicator
	//string, described format of 'user_data' parameter,
	//taken by simulate function
	const char* format_string;
};
//initialize and destroy functions
static void kedr_simulation_point_init(struct kedr_simulation_point* point,
    const char* name, const char* format_string);
static void kedr_simulation_point_destroy(struct kedr_simulation_point* point);

static void kedr_simulation_point_finalize(wobj_t* obj);

// List of simulation points.
static LIST_HEAD(sim_points_list);

// Spinlock which protect list of simulation points from concurrent read and write
static DEFINE_SPINLOCK(sim_points_spinlock);

// Auxiliary functions

/*
 * Return simulation point with given name or NULL.
 *
 * Should be executed under spinlock taken.
 */

static struct kedr_simulation_point* fsim_lookup_point(const char* name);

/*
 *  Verify, whether data, which format is described in
 *  'point_format_string', will be correctly interpreted by indicator,
 *  which expect data in 'indicator_format_string' format.
 * 
 *  Return not 0 on success, 0 otherwise.
 *
 *  May work in atomic context.
 */

static int
is_data_format_compatible(	const char* point_format_string,
							const char* indicator_format_string);

//////Implementation of exported function//////////////////////

/*
 * Register simulation point with name 'point_name'.
 * 
 * Initially (before calling kedr_fsim_set_indicator)
 * point use no fault indicator, and
 * kedr_fsim_simulate for it return always 0.
 * 
 * Returning value may be used in kedr_fsim_simulate
 *  and kedr_fsim_point_unregister.
 * 
 * format_string should describe real format of user_data,
 * which may be passed to the kedr_fsim_simulate().
 * This format string is used to verify, whether particular indicator
 * fits for this simulation point.
 *
 * It is caller who responsible for passing user_data in correct format
 * to the kedr_fsim_simulate().
 *
 * If this name has already used for another point, returns NULL.
 */

struct kedr_simulation_point* 
kedr_fsim_point_register(const char* point_name,
	const char* format_string)
{
    unsigned long flags;
    
	struct kedr_simulation_point* new_point = kmalloc(sizeof(*new_point), GFP_KERNEL);
    if(new_point == NULL)
    {
        print_error0("Cannot allocate 'struct kedr_simulation_point'.");
        return NULL;
    }
    kedr_simulation_point_init(new_point, point_name, format_string);
    
	spin_lock_irqsave(&sim_points_spinlock, flags);
	if(fsim_lookup_point(point_name) != NULL)
    {
        spin_unlock_irqrestore(&sim_points_spinlock, flags);
        print_error("Point with name '%s' already registered.", point_name);
        kedr_simulation_point_destroy(new_point);
        kfree(new_point);
        return NULL;
    }
    list_add_tail(&new_point->list, &sim_points_list);
    spin_unlock_irqrestore(&sim_points_spinlock, flags);
    debug("Simulation point with name '%s' was registered(format string is '%s').",
        point_name, format_string);
	return new_point;
}
EXPORT_SYMBOL(kedr_fsim_point_register);

/*
 * Unregister point, making its name free for use.
 * 
 * Deregistration process is perfomed automatically
 * when the module, declared this point, is unloaded.
 */

void kedr_fsim_point_unregister(struct kedr_simulation_point* point)
{
	unsigned long flags;
    debug("Simulation point with name '%s' go to be unregistered.",
        point->name);

    spin_lock_irqsave(&sim_points_spinlock, flags);
	list_del(&point->list);
    spin_unlock_irqrestore(&sim_points_spinlock, flags);
    wobj_unref_final(&point->obj);
}
EXPORT_SYMBOL(kedr_fsim_point_unregister);

/*
 * Invoke simulation indicator, which set for this point,
 * and return its result.
 * 
 * Format of 'user_data' should correspond to the format string
 * used when point was registered.
 */

int kedr_fsim_simulate(struct kedr_simulation_point* point,
	void* user_data)
{
    int result;
    struct fault_indicator_info* indicator;
    // function should't be called with concurrently with deleting point,
    //so no need to spinlock, but see kedr_fsim_indicator_set()
    wobj_t* indicator_obj = wobj_weak_ref_get(&point->wri);
    if(indicator_obj == NULL) return 0;//no indicator was set
    
    indicator = container_of(indicator_obj, struct fault_indicator_info, obj);
    result = indicator->fi(indicator->indicator_state, user_data);
    wobj_unref(indicator_obj);
    return result;
}
EXPORT_SYMBOL(kedr_fsim_simulate);

/*
 * Set fault indicator 'fi' for simulation point.
 * 
 * 'format string' describe format of 'user_data',
 * which indicator can process.
 *
 * 'indicator_state' will be passed to the indicator function,
 *  when it will be called.
 * 
 * 'destroy' function will be called when state 
 * will no longer been used (e.g., indicator was changed).
 * 
 *  When module 'm' is unloaded, indicator is cleared for this
 *  simulation point.
 *
 * If point is not registered, return -1.
 * 
 * If indicator format string do not correspond to format string of
 * simulation point, return 1.
 * 
 * Otherwise set given indicator as current indicator,
 * used by point, and return 0.
 */

int kedr_fsim_indicator_set(const char* point_name,
	kedr_fsim_fault_indicator fi, const char* format_string,
	struct module* m,
	void* indicator_state, kedr_fsim_destroy_indicator_state destroy)
{
	unsigned long flags;

    struct kedr_simulation_point* point;
    
    wobj_t* current_indicator_obj;
    struct fault_indicator_info* new_indicator;
    //wobj_t* new_indicator_obj;

    new_indicator = kmalloc(sizeof(*new_indicator), GFP_KERNEL);
    if(new_indicator == NULL)
    {    
        print_error0("Cannot allocate memory fo indicator.");
        return -1;
    }   
    spin_lock_irqsave(&sim_points_spinlock, flags);
    point = fsim_lookup_point(point_name);
    if(point == NULL)
    {
        spin_unlock_irqrestore(&sim_points_spinlock, flags);
        kfree(new_indicator);
        print_error("Simulation point with name '%s' is not exist.", point_name);
        return -1;
    }
    if(!is_data_format_compatible(point->format_string,
        format_string))
    {
        spin_unlock_irqrestore(&sim_points_spinlock, flags);
        kfree(new_indicator);
        print_error("Indicator format string '%s' is not compatible with format string "
            "of simulation point '%s' ('%s').", format_string, point_name, point->format_string);
        return -1;
    }
    current_indicator_obj = wobj_weak_ref_get(&point->wri);
    if(current_indicator_obj != NULL)
    {
        wobj_weak_ref_clear(&point->wri);
        //at this state, simulate() see absence of the indicator, if do not use lock.
        //should be fixed
    }
    fault_indicator_info_init(new_indicator, fi, indicator_state,
        destroy, m);
    wobj_weak_ref_init(&point->wri, &new_indicator->obj, NULL);
    spin_unlock_irqrestore(&sim_points_spinlock, flags);
    if(current_indicator_obj != NULL)
    {
        fault_indicator_info_delete_now(current_indicator_obj);
    }
    debug("New indicator was set for point '%s'.", point_name);
    return 0;
}
EXPORT_SYMBOL(kedr_fsim_indicator_set);

int kedr_fsim_indicator_clear(const char* point_name)
{
	unsigned long flags;

    struct kedr_simulation_point* point;
    wobj_t* current_indicator_obj;

    spin_lock_irqsave(&sim_points_spinlock, flags);
    point = fsim_lookup_point(point_name);
    if(point == NULL)
    {
        spin_unlock_irqrestore(&sim_points_spinlock, flags);
        print_error("Simulation point with name '%s' is not exist.", point_name);
        return -1;
    }
    current_indicator_obj = wobj_weak_ref_get(&point->wri);

    if(current_indicator_obj != NULL)
    {
        wobj_weak_ref_clear(&point->wri);
        
    }
    spin_unlock_irqrestore(&sim_points_spinlock, flags);
    if(current_indicator_obj != NULL)
    {
        fault_indicator_info_delete_now(current_indicator_obj);
    }
    debug("Indicator was cleared for point '%s'.", point_name);
    return 0;
}

EXPORT_SYMBOL(kedr_fsim_indicator_clear);

///////////Implementation of auxiliary functions//////////////////////////
void fault_indicator_info_init(struct fault_indicator_info* fii,
    kedr_fsim_fault_indicator fi,
    void* indicator_state,
    kedr_fsim_destroy_indicator_state destroy,
    struct module* m)
{
    fii->fi = fi;
    fii->indicator_state = indicator_state;
    fii->destroy = destroy;
    wobj_init(&fii->obj, fault_indicator_info_finalize);
    wmodule_weak_ref_init(&fii->wrm, m, fault_indicator_info_on_module_unload);
}

void fault_indicator_info_destroy(struct fault_indicator_info* fii)
{
}

void fault_indicator_info_finalize(wobj_t* obj)
{
    struct fault_indicator_info* fii = container_of(obj, struct fault_indicator_info, obj);
    if(fii->destroy)
        fii->destroy(fii->indicator_state);
    fault_indicator_info_destroy(fii);
    kfree(fii);
}

void fault_indicator_info_on_module_unload(wmodule_weak_ref_t* wrm)
{
    //Really, this callback is the only contains strong reference to the indicator :)
    //So, who wish to delete indicator, should work with its 'wrm' field
    struct fault_indicator_info* fii = container_of(wrm, struct fault_indicator_info, wrm);
    wobj_unref_final(&fii->obj);
}

void fault_indicator_info_delete_now(wobj_t* indicator_object)
{
    struct module* m;
    struct fault_indicator_info* fii = 
        container_of(indicator_object, struct fault_indicator_info, obj);
    m = wmodule_weak_ref_get(&fii->wrm);
    if(m == NULL)
    {
        //module m already unloaded, and indicator currently destroyed
        //wait it
        wmodule_wait_callback_t wait_callback;
        
        wmodule_wait_callback_prepare(&wait_callback, &fii->wrm);
        wobj_unref(&fii->obj);
        wmodule_wait_callback_wait(&wait_callback);
    }
    else
    {
        wmodule_weak_ref_clear(&fii->wrm);
        wobj_unref_final(&fii->obj);
        wmodule_unref(m);
    }
}

//
void kedr_simulation_point_init(struct kedr_simulation_point* point,
    const char* name, const char* format_string)
{
    point->name = name;
    point->format_string = format_string;
    
    wobj_init(&point->obj, kedr_simulation_point_finalize);
    wobj_weak_ref_init(&point->wri, NULL, NULL);
}
//
void kedr_simulation_point_destroy(struct kedr_simulation_point* point)
{
}

void kedr_simulation_point_finalize(wobj_t* obj)
{
    wobj_t* indicator_object;
    struct kedr_simulation_point* point = 
        container_of(obj, struct kedr_simulation_point, obj);
    indicator_object = wobj_weak_ref_get(&point->wri);
    //if
    if(!indicator_object == NULL)
    {
        fault_indicator_info_delete_now(indicator_object);
    }
    kedr_simulation_point_destroy(point);
    kfree(point);
}

//
int
is_data_format_compatible(	const char* point_format_string,
							const char* indicator_format_string)
{
	if(indicator_format_string == NULL
		|| *indicator_format_string == '\0')
	{
		//always compatible
		return 1;
	}
	else if(point_format_string == NULL
		|| *point_format_string == '\0')
	{
		//no data are passed, but indicator expects something
		return 0;
	}
	// simple verification, may be changed in the future
	return strncmp(point_format_string, indicator_format_string,
		strlen(indicator_format_string)) == 0;
}

struct kedr_simulation_point* fsim_lookup_point(const char* name)
{
	struct kedr_simulation_point* point;
	list_for_each_entry(point, &sim_points_list, list)
	{
		if(strcmp(name, point->name) == 0) return point;
	}
	return NULL;
}

static void
fsim_cleanup_module(void)
{
	while(!list_empty(&sim_points_list))
	{
		struct kedr_simulation_point* point =
            list_first_entry(&sim_points_list, struct kedr_simulation_point, list);
		kedr_fsim_point_unregister(point);
	}
	module_weak_ref_destroy();
}

static int __init
fsim_init_module(void)
{
	return module_weak_ref_init();
}


module_init(fsim_init_module);
module_exit(fsim_cleanup_module);
