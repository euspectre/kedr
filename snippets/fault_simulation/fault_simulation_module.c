#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/list.h>		/* list functions */

#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include "module_weak_ref.h"
#include "fault_simulation.h"

/*
 * Information about indicator, that needed for correct work of simulate
 */

struct fault_indicator_info
{
	kedr_fsim_fault_indicator fi;
	struct module* m;
	void* indicator_state;
	kedr_fsim_destroy_indicator_state destroy;
};

struct kedr_simulation_point
{
	struct list_head list;
	
	const char* name;
	//string, described format of 'user_data' parameter,
	//taken by simulate function
	const char* format_string;
	
	struct fault_indicator_info current_indicator;
};
// List of simulation points.
LIST_HEAD(sim_points);

// Auxiliary functions

// Return simulation point with given name or NULL.
struct kedr_simulation_point* fsim_lookup_point(const char* name);

/*
 *  Verify, whether data, which format is described in
 *  'point_format_string', will be correctly interpreted by indicator,
 *  which expect data in 'indicator_format_string' format.
 * 
 *  Return not 0 on success, 0 otherwise.
 */

static int
is_data_format_compatible(	const char* point_format_string,
							const char* indicator_format_string);
// Same as kedr_fsim_set_indicator but use point itself instead of its name
static int fsim_set_indicator_internal(struct kedr_simulation_point* point,
	kedr_fsim_fault_indicator fi, const char* format_string,
	struct module* m,
	void* indicator_state, kedr_fsim_destroy_indicator_state destroy);

// Unset indicator.
// Called when module, provided that indicator, is unloaded
static void unset_indicator_callback(struct module* m,
	struct fault_indicator_info* current_indicator);

static void
fsim_cleanup_module(void)
{
	while(!list_empty(&sim_points))
	{
		kedr_fsim_point_unregister(list_entry(
			sim_points.next, struct kedr_simulation_point, list));
	}
	module_weak_ref_destroy();
}

static int __init
fsim_init_module(void)
{
	return module_weak_ref_init();
}

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
	struct kedr_simulation_point* new_point;

	if(fsim_lookup_point(point_name)) return NULL;
	
	new_point = kmalloc(sizeof(*new_point), GFP_KERNEL);
	
	new_point->name = point_name;
	new_point->format_string = format_string;
	
	new_point->current_indicator.fi = NULL;
	new_point->current_indicator.m = NULL;
	new_point->current_indicator.indicator_state = NULL;
	new_point->current_indicator.destroy = NULL;
	
	list_add_tail(&new_point->list, &sim_points);
	return new_point;
}
EXPORT_SYMBOL(kedr_fsim_point_register);

/*
 * Unregister point, making its name free for use.
 * 
 * Deregistration process is perfomed automatically
 * when this module unload.
 */

void kedr_fsim_point_unregister(struct kedr_simulation_point* point)
{
	fsim_set_indicator_internal(point, NULL, NULL, NULL, NULL, NULL);
	list_del(&point->list);
	
	kfree(point);
	
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
	return point->current_indicator.fi 
			? point->current_indicator.fi(
				point->current_indicator.indicator_state,
				user_data)
			: 0;
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

int kedr_fsim_set_indicator(const char* point_name,
	kedr_fsim_fault_indicator fi, const char* format_string,
	struct module* m,
	void* indicator_state, kedr_fsim_destroy_indicator_state destroy)
{
	struct kedr_simulation_point* point = fsim_lookup_point(point_name);
	
	if(point == NULL) return -1;

	return fsim_set_indicator_internal(point, fi, format_string,
		m, indicator_state, destroy);
}
EXPORT_SYMBOL(kedr_fsim_set_indicator);

static int
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


static int 
fsim_set_indicator_internal(struct kedr_simulation_point* point,
	kedr_fsim_fault_indicator fi, const char* format_string,
	struct module* m,
	void* indicator_state, kedr_fsim_destroy_indicator_state destroy)
{
	struct fault_indicator_info* current_indicator =
		&point->current_indicator;
	if(!is_data_format_compatible(point->format_string,
								format_string))
	{
		return 1;
	}
	
	if(current_indicator->destroy)
		current_indicator->destroy(current_indicator->indicator_state);
	if(current_indicator->m)
		module_weak_unref(current_indicator->m, 
			(destroy_notify)unset_indicator_callback, current_indicator);
	
	current_indicator->fi = fi;
	current_indicator->m = m;
	current_indicator->indicator_state = indicator_state;
	current_indicator->destroy = destroy;

	if(m)
		module_weak_ref(m, 
			(destroy_notify)unset_indicator_callback, current_indicator);
	return 0;
}

static void unset_indicator_callback(struct module* m,
	struct fault_indicator_info* current_indicator)
{
	if(current_indicator->destroy)
		current_indicator->destroy(current_indicator->indicator_state);

	current_indicator->fi = NULL;
	current_indicator->m = NULL;
	current_indicator->indicator_state = NULL;
	current_indicator->destroy = NULL;
	//Because it is result on module unloaded, doesn't perform weak_unref
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

module_init(fsim_init_module);
module_exit(fsim_cleanup_module);
