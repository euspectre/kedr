#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/list.h>		/* list functions */

#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include "module_weak_ref.h"
#include "fault_simulation.h"

struct kedr_simulation_point
{
	struct list_head list;
	const char* name;
	fault_indicator fi;
	struct module* m;
	void* user_data;
	destroy_data destroy;
};
// List of simulation points.
LIST_HEAD(sim_points);

// Auxiliary functions

// Return simulation point with given name or NULL.
struct kedr_simulation_point* fsim_lookup_point(const char* name);
// Same as kedr_fsim_set_indicator but use point itself instead of its name
static void fsim_set_indicator_internal(struct kedr_simulation_point* point,
	fault_indicator fi, struct module* m,
	void* user_data, destroy_data destroy);


// Unset indicator.
// Called when module, provided that indicator, is unloaded
static void unset_indicator_callback(struct module* m,
	struct kedr_simulation_point* point);

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
 * Returning value may be used in kedr_fsim_simulate
 *  and kedr_fsim_point_unregister.
 * 
 * If this name has already used for another point, returns NULL.
 */

struct kedr_simulation_point* 
kedr_fsim_point_register(const char* point_name)
{
	struct kedr_simulation_point* new_point;

	if(fsim_lookup_point(point_name)) return NULL;
	
	new_point = kmalloc(sizeof(*new_point), GFP_KERNEL);
	
	new_point->name = point_name;
	new_point->fi = NULL;
	new_point->m = NULL;
	new_point->user_data = NULL;
	new_point->destroy = NULL;
	
	list_add_tail(&new_point->list, &sim_points);
	return new_point;
}
EXPORT_SYMBOL(kedr_fsim_point_register);
/*
 * Unregister point, making its name free for use.
 * 
 * This function will be automatically called when support module unload.
 */

void kedr_fsim_point_unregister(struct kedr_simulation_point* point)
{
	fsim_set_indicator_internal(point, NULL, NULL, NULL, NULL);
	list_del(&point->list);
	
	kfree(point);
	
}
EXPORT_SYMBOL(kedr_fsim_point_unregister);
/*
 * Invoke simulation indicator, which set for this point,
 * and return its result.
 */

int kedr_fsim_simulate(struct kedr_simulation_point* point)
{
	return point->fi ? point->fi(point->user_data) : 0;
}
EXPORT_SYMBOL(kedr_fsim_simulate);
/*
 * Set fault indicator for point.
 * 
 * If point is not registered, return -1.
 * Otherwise set indicator fi as current indicator,
 * used by point, and return 0.
 *
 * 'user_data' will be passed to the indicator function,
 *  when it will be called.
 * 
 * 'destroy' function will be called when user_data 
 * will no longer been used (e.g., indicator was changed).
 */

int kedr_fsim_set_indicator(const char* point_name,
	fault_indicator fi, struct module* m,
	void* user_data, destroy_data destroy)
{
	struct kedr_simulation_point* point = fsim_lookup_point(point_name);
	
	if(point == NULL) return -1;

	fsim_set_indicator_internal(point, fi, m, user_data, destroy);
	return 0;
}
EXPORT_SYMBOL(kedr_fsim_set_indicator);

static void fsim_set_indicator_internal(struct kedr_simulation_point* point,
	fault_indicator fi, struct module* m,
	void* user_data, destroy_data destroy)
{
	if(point->destroy)
		point->destroy(point->user_data);
	if(point->m)
		module_weak_unref(point->m, 
			(destroy_notify)unset_indicator_callback, point);
	
	point->fi = fi;
	point->m = m;
	point->user_data = user_data;
	point->destroy = destroy;

	if(m)
		module_weak_ref(m, 
			(destroy_notify)unset_indicator_callback, point);
}

static void unset_indicator_callback(struct module* m,
	struct kedr_simulation_point* point)
{
	if(point->destroy)
		point->destroy(point->user_data);

	point->fi = NULL;
	point->m = NULL;
	point->user_data = NULL;
	point->destroy = NULL;
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
