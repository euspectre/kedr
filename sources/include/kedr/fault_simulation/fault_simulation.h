#ifndef FAULT_SIMULATION_H
#define FAULT_SIMULATION_H

#ifndef __KERNEL__
#error "This header is only for kernel code"
#endif

#include <linux/debugfs.h> /* struct dentry */

/*
 * Format description of 'user_data' parameter,
 * passed from point to indicator function at simulation stage:
 * 
 * - "" or NULL means, that user_data parameter is not used.
 * - "type" means, that 'user_data' points to variable of type 'type'
 * - "type1, type2..." means, that 'user_data' points to struct
 * 
 *  struct{
 *   type1 var1;
 *   type2 var2;
 *   ...
 *  };
 * 
 */

/*
 * Type of point, where need to simulate.
 */

struct kedr_simulation_point;

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
 *
 */

struct kedr_simulation_point* 
kedr_fsim_point_register(const char* point_name,
	const char* format_string);

/*
 * Unregister point, making its name free for use, and release resources.
 */

void kedr_fsim_point_unregister(struct kedr_simulation_point* point);


/*
 * Type of object, which decide, need to fault at the point, or needn't
 */

struct kedr_simulation_indicator;

/*
 * Register indicator.
 *
 * 'indicator_name' - name of the indicator created.
 *
 * 'simulate' - function which will be called at simulation stage
 *
 * 'format_string' - format of 'user_data' parameter, taken by 'simulate'
 *
 * 'create_instance' - function which will be called for create indicator instance
 * for particular point.
 *
 * 'destroy_instance' - function which will be called when indicator instance
 * should be unset for particular point
 *
 * Return not-null pointer, which may be used for unregister of indicator.
 *
 * If cannot create indicator for some reason(e.g., 'indicator_name' already used as name of other indicator),
 * return NULL.
 */

struct kedr_simulation_indicator* 
kedr_fsim_indicator_register(const char* indicator_name,
	int (*simulate)(void* indicator_state, void* user_data),
    const char* format_string,
    int (*create_instance)(void** indicator_state, const char* params, struct dentry* control_directory),
    void (*destroy_instance)(void* indicator_state)
);

/*
 * Unregister indicator, making its name free for use, and release resources.
 *
 * Also clear indicator for points, which are currently using insances of this indicator.
 */

void kedr_fsim_indicator_unregister(struct kedr_simulation_indicator* indicator);

/*
 * Create and set instance of indicator with name 'indicator_name',
 * for point with name 'point_name'.
 *
 * Return 0 on success, not 0 - on fail.
 *
 * Note: there is a possibility, that kedr_simulation_point_simulate() will be called as with no indicator
 * while executing kedr_simulation_point_set_indicator().
 */

int kedr_fsim_point_set_indicator(const char* point_name,
    const char* indicator_name, const char* params);

/*
 * Clear and destroy indicator instance for given point, if it was set.
 */

int kedr_fsim_point_clear_indicator(const char* point_name);

/*
 * Call indicator, which was set for this point, and return result of indicator's
 * 'simulate' function.
 *
 * If indicator wasn't set for this point, return 0.
 */

int kedr_fsim_point_simulate(struct kedr_simulation_point* point,
    void *user_data);


#endif