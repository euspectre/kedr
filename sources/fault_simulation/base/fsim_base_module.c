#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/list.h>		/* list functions */

#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <kedr/module_weak_ref/module_weak_ref.h>
#include <kedr/fault_simulation/fsim_base.h>

#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

/*
 * Information about indicator, that needed for correct work of simulate
 */

struct fault_indicator_info
{
	//indicator function
	kedr_fsim_fault_indicator fi;
    //module, which provide function and other data for its work
	struct module* m;
    //current state of the indicator(may be used by indicator function)
	void* indicator_state;
    //function to be called when module 'm' is unloaded
	kedr_fsim_destroy_indicator_state destroy;
};



/*
 * Default indicator is {NULL, NULL, NULL, NULL}.
 *
 * It considered to always return 0('fi' is NULL), to always existed('m' is NULL),
 * and its state not need to destroy('destroy' is NULL).
 */

/*
 * Simulation point
 */
struct kedr_simulation_point
{
	struct list_head list;
	//name of the point
	const char* name;
	//string, described format of 'user_data' parameter,
	//taken by simulate function
	const char* format_string;
	//current indicator, used by point
	struct fault_indicator_info current_indicator;//really, this field should be pointer, and be protected by RCU.
};
//initialize and destroy functions
void kedr_simulation_point_init(struct kedr_simulation_point* point,
    const char* name, const char* format_string);
void kedr_simulation_point_destroy(struct kedr_simulation_point* point);

// List of simulation points.
static LIST_HEAD(sim_points);

// Spinlock which protect list of simulation points from concurrent read and write
static spinlock_t sim_points_spinlock;
/*
 * Mutex protect list from concurrent writes.
 *
 * That is, indicator my be in the intermediate state. Reading it(e.g, fault simulating)
 * is not affected by this 'intermediate', but rewritting may corrupt program logic.
 * The mutex prevents such state from rewritting.
 */
static struct mutex sim_points_mutex;
// Auxiliary functions

/*
 * Return simulation point with given name or NULL.
 *
 * Should be executed under spinlock taken.
 */

struct kedr_simulation_point* fsim_lookup_point(const char* name);

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

/*
 * Clear indicator for given point.
 *
 * Internal function, should be used under spinlock and mutex taken.
 * May reaquire spinlock, but mutex is not released.
 *
 * 'flags' - result of spin_lock_irqsave, need for reaquire spinlock.
 */

static void fsim_indicator_clear_internal(struct kedr_simulation_point* point, unsigned long *flags);

/*
 * Callback, which unset indicator for the point.
 *
 * Called when module, provided that indicator, is unloaded.
 *
 * This function is the only one, who can violate sim_points_mutex logic.
 * Other functions know about this.
 */

static void fsim_indicator_clear_callback(struct module* m,
	struct fault_indicator_info* current_indicator);

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
    list_add_tail(&new_point->list, &sim_points);
    spin_unlock_irqrestore(&sim_points_spinlock, flags);

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
    //Generally speaking, using simulation point concurrently with its deleting
    // is error. But we try to correctly handle this case.
    mutex_lock(&sim_points_mutex);
    spin_lock_irqsave(&sim_points_spinlock, flags);
	fsim_indicator_clear_internal(point, &flags);
	list_del(&point->list);
    spin_unlock_irqrestore(&sim_points_spinlock, flags);
    kedr_simulation_point_destroy(point);
	kfree(point);
    mutex_unlock(&sim_points_mutex);
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
	unsigned long flags;
    int result;

    spin_lock_irqsave(&sim_points_spinlock, flags);
	result = point->current_indicator.fi 
			? point->current_indicator.fi(
				point->current_indicator.indicator_state,
				user_data)
			: 0;
    spin_unlock_irqrestore(&sim_points_spinlock, flags);

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
    struct fault_indicator_info* current_indicator;

    mutex_lock(&sim_points_mutex);
    spin_lock_irqsave(&sim_points_spinlock, flags);
	point = fsim_lookup_point(point_name);
    if(point == NULL)
    {
        spin_unlock_irqrestore(&sim_points_spinlock, flags);
        mutex_unlock(&sim_points_mutex);
        return -1;//point doesn't exist
    }
    if(!is_data_format_compatible(point->format_string,
        format_string))
    {
        spin_unlock_irqrestore(&sim_points_spinlock, flags);
        mutex_unlock(&sim_points_mutex);
        return -1;//formats of point and indicator 'user_data' are not compatible
    }
    fsim_indicator_clear_internal(point, &flags);
    //now old indicator is cleared, set new one
    current_indicator = &point->current_indicator;
    
    current_indicator->fi = fi;
    current_indicator->m = m;
    current_indicator->indicator_state = indicator_state;
    current_indicator->destroy = destroy;

    /*
     * Everything is done, except scheduling callback.
     * But module_weak_ref cannot be called from interrupt context, so release spinlock.
     */

    spin_unlock_irqrestore(&sim_points_spinlock, flags);
    
    if((current_indicator->m != NULL) && (current_indicator->destroy != NULL))
    {
        module_weak_ref(current_indicator->m, (destroy_notify)fsim_indicator_clear_callback,
            current_indicator);
    }
    //State is stable now, release mutex
    mutex_unlock(&sim_points_mutex);

    return 0;
}
EXPORT_SYMBOL(kedr_fsim_indicator_set);

int kedr_fsim_indicator_clear(const char* point_name)
{
	unsigned long flags;

    struct kedr_simulation_point* point;

    mutex_lock(&sim_points_mutex);
    spin_lock_irqsave(&sim_points_spinlock, flags);
	point = fsim_lookup_point(point_name);
    if(point == NULL)
    {
        spin_unlock_irqrestore(&sim_points_spinlock, flags);
        mutex_unlock(&sim_points_mutex);
        return -1;//point doesn't exist
    }
    fsim_indicator_clear_internal(point, &flags);
    
    spin_unlock_irqrestore(&sim_points_spinlock, flags);
    mutex_unlock(&sim_points_mutex);

    return 0;
}

EXPORT_SYMBOL(kedr_fsim_indicator_clear);

///////////Implementation of auxiliary functions//////////////////////////
//
void kedr_simulation_point_init(struct kedr_simulation_point* point,
    const char* name, const char* format_string)
{
    point->name = name;
    point->format_string = format_string;
    
    point->current_indicator.fi = NULL;
	point->current_indicator.m = NULL;
	point->current_indicator.indicator_state = NULL;
	point->current_indicator.destroy = NULL;
}
//
void kedr_simulation_point_destroy(struct kedr_simulation_point* point)
{
}


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
// under mutex and lock taken
void fsim_indicator_clear_internal(struct kedr_simulation_point* point, unsigned long *flags)
{
    struct fault_indicator_info* current_indicator;

    current_indicator = &point->current_indicator;
    //destroy indicator state if needed, and clear indicator fields
    //But at the beginning - cancel scheduled clear function, if it was
    if((current_indicator->m == NULL) || (current_indicator->destroy == NULL)
        || (module_weak_unref(current_indicator->m,
                (destroy_notify)fsim_indicator_clear_callback,
                current_indicator) == 0)
    )
    {
        if(current_indicator->destroy)
            current_indicator->destroy(current_indicator->indicator_state);

        current_indicator->fi = NULL;
        current_indicator->m = NULL;
        current_indicator->indicator_state = NULL;
        current_indicator->destroy = NULL;
    }
    else
    {
        //release spinlock for allow callback function to finish
        spin_unlock_irqrestore(&sim_points_spinlock, *flags);
        module_weak_ref_wait();
        //indicator is cleared by callback, reaquire spinlock
        spin_lock_irqsave(&sim_points_spinlock, *flags);
    }

}
//
void fsim_indicator_clear_callback(struct module* m,
	struct fault_indicator_info* current_indicator)
{
	unsigned long flags;
	spin_lock_irqsave(&sim_points_spinlock, flags);

    //module_weak_ref shouldn't be used when indicator has no 'destroy' function
    BUG_ON(!current_indicator->destroy);
	current_indicator->destroy(current_indicator->indicator_state);
	current_indicator->fi = NULL;
	current_indicator->m = NULL;
	current_indicator->indicator_state = NULL;
	current_indicator->destroy = NULL;

    spin_unlock_irqrestore(&sim_points_spinlock, flags);
}

struct kedr_simulation_point* fsim_lookup_point(const char* name)
{
	struct kedr_simulation_point* point;
	list_for_each_entry(point, &sim_points, list)
	{
		if(strcmp(name, point->name) == 0) return point;
	}
	return NULL;
}

static void
fsim_cleanup_module(void)
{
	while(!list_empty(&sim_points))
	{
		kedr_fsim_point_unregister(list_entry(
			sim_points.next, struct kedr_simulation_point, list));
	}
    mutex_destroy(&sim_points_mutex);
	module_weak_ref_destroy();
}

static int __init
fsim_init_module(void)
{
	spin_lock_init(&sim_points_spinlock);
    mutex_init(&sim_points_mutex);
	return module_weak_ref_init();
}


module_init(fsim_init_module);
module_exit(fsim_cleanup_module);
