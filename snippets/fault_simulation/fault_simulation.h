/*
 * This file contains common declarations
 * of fault simulation architecture.
 */

#ifndef FAULT_SIMULATION_H
#define FAULT_SIMULATION_H

#include <linux/module.h>	/* struct module definition */

/* 
 * Type of indicator function, used for fault simulation.
 * 
 * Returning value other than 0 means "simulate fault".
 * 
 * [NB] Precise meaning of user_data pasrameter is 
 * currently undefine.
 */
typedef int (*fault_indicator)(void* user_data);

/*
 * Type of function, which called when data is no longer needed.
 */

typedef void (*destroy_data)(void* data);

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
 * If this name has already used for another point, returns NULL.
 */

struct kedr_simulation_point* kedr_fsim_point_register(const char* point_name);

/*
 * Unregister point, making its name free for use.
 * 
 * This function will be automatically called when support module unload.
 */

void kedr_fsim_point_unregister(struct kedr_simulation_point* point);

/*
 * Invoke simulation indicator, which set for this point,
 * and return its result.
 */

int kedr_fsim_simulate(struct kedr_simulation_point* point);

/*
 * Set fault indicator for point.
 * 
 * If point is not registered, return -1.
 * Otherwise set indicator fi as current indicator,
 * used by point, and return 0.
 *
 * Module 'm' will be locked while indicator 'fi' is set for this point.
 * 
 * 'user_data' will be passed to the indicator function,
 *  when it will be called.
 * 
 * 'destroy' function will be called when user_data 
 * will no longer been used (e.g., indicator was changed).
 */

int kedr_fsim_set_indicator(const char* point_name,
	fault_indicator fi, struct module* m,
	void* user_data, destroy_data destroy);

// Additional functions may be here.


#endif /* FAULT_SIMULATION_H */
