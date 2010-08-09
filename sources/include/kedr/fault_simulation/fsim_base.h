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
 * 'indicator state' points to current state of indicator.
 * Format of state depends on indicator function.
 * 
 * 'user_data' points to data, passed to kedr_fsim_simulate function.
 * Format of this data depends on simulation point.
 */

typedef int (*kedr_fsim_fault_indicator)(void* indicator_state, void* user_data);

/*
 * Format description of user_data parameter:
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
 * Type of function, which called when indicator is no longer needed.
 */

typedef void (*kedr_fsim_destroy_indicator_state)(void* state);

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
 */

struct kedr_simulation_point* 
kedr_fsim_point_register(const char* point_name,
	const char* format_string);

/*
 * Unregister point, making its name free for use.
 * 
 * Deregistration process is perfomed automatically
 * when this module unload.
 */

void kedr_fsim_point_unregister(struct kedr_simulation_point* point);

/*
 * Invoke simulation indicator, which set for this point,
 * and return its result.
 * 
 * Format of 'user_data' should correspond to the format string
 * used when point was registered.
 *
 * May be executed in the atomic context(and in the interruptible context?).
 */

int kedr_fsim_simulate(struct kedr_simulation_point* point,
	void* user_data);

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
 *
 * Because point may be used anywere in the program, 
 * 'fi' should can correctly work in atomic context(and in the interrupt context?).
 */

int kedr_fsim_indicator_set(const char* point_name,
	kedr_fsim_fault_indicator fi, const char* format_string,
	struct module* m,
	void* indicator_state, kedr_fsim_destroy_indicator_state destroy);

// Additional functions may be here.

// Clear indicator for given point
int kedr_fsim_indicator_clear(const char* point_name);

#endif /* FAULT_SIMULATION_H */
