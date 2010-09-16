#ifndef FSIM_INDICATOR_MANAGER_H
#define FSIM_INDICATOR_MANAGER_H

/*
 * API for managing indicator functions.
 *
 * Base implementation of fault simulation points and indicators
 * is powerful in kernel area - one can implement some function
 * in its own kernel module, and use it immediatly as indicator function for
 * some simulation point.
 *
 * But mechanism for setting indicators in this form
 * cannot be used, generally, from user space, because:
 *
 * 1)indicator function cannot be implemented in user space
 *
 * 2)moreover, even when user space wish to use existent
 *  indicator function in the kernel, it doesn't know address of this function;
 *  same for destroying function of indicator.
 *
 * 3)state of indicator function may contains, e.g., reference to memory, allocated in the kernel,
 *  this reference also is unknown in user space.
 *
 * 4)user may mistake, and set the indicator function for simulation point, 
 *  when this indicator function cannot correctly process parameters, passed to the point at simulation stage.
 *  This will lead, probably, to kernel crash when this indicator function be called for simulation.
 *
 * First problem is unavoidable - one cannot force kernel to use user-space function.
 * But setting preexistent functions as indicators may be effective implemented in user space.
 *
 * Some function may be registered in the kernel, as designed for fault simulation indicator.
 * After registration, this function become named, so setting it as indicator for some point
 * become available from user space - one should only pass its name to the kernel(fix one part of problem 2).
 *
 * Also, information about indicator function's parameters passed to the registration procedure.
 * So, no one can register this indicator function for simulation point, which accept incompatible parameters
 * (fix problem 4).
 *
 * This indicator function may work with 'indicator_state' parameter of definite type,
 * but instead of passing concrete state to the registration procedure, one should pass another function, which
 * used for create initial value for state, when this indicator function will be registered for some point.
 * This have several advantages over passing state itself to the indicator registration procedure:
 *
 *  -if indicator function modify state at simulation stage, this drop dependence between different simulations points,
 *   when them use same indicator. Different simulation points are expected to be independent from each other, otherwise
 *   one should use one simulation point in two or more places.
 *
 *  -if start state is large, should be allocated in the memory or need long computations, this approach save resources
 *   by delaying creation of this state to the moment, when indicator will be used for some point. Stating that
 *   some function may be used as indicator doesn't imply that it will be really used.
 *
 *  -the main advantage - function, which create state, accept parameters. So, start indicator_state may have different content
 *   for different registration processes of indicator function. This make indicator to cover not only one situation,
 *   but to cover a whole class of situation.
 *
 * For use from the user-space, parameters for function, which create indicator_state for concrete indicator, are raw array of bytes
 * and length of this array. Such parameters are most common, that may be passed from the user space to the kernel(fix problem 3).
 * Raw array itself may incapsulate numbers, strings, etc. 
 * But, it couldn't contain pointers to the kernel functions or kernel data - user space simply
 * doesn't now them (and it is dangerous to the kernel accept such parameters from user space).
 */

#ifdef __KERNEL__

#include <kedr/fault_simulation/fsim_base.h>

/*
 * Type of function which initialize indicator state according
 * to parameters. Parameters are represented by array of bytes
 * ('params') of some length (params_len).
 * 
 * This parameter representation is suitable for to be transfered
 * from the user space to the kernel.
 * 
 * On success, function should return 0.
 * 
 * If format of parameters is invalid or data parameter for
 * indicator cannot be created for some others reasons,
 * function should return not 0(in that case indicator_state have no meaning).
 */

typedef int (*kedr_fsim_init_indicator_state)(const void* params,
	size_t params_len, void** indicator_state);

/*
 * Create named indicator, which incapsulate indicator function
 * with some other attributes(see description at the top of the header).
 *
 * 'indicator_name' - name of the indicator created.
 *
 * Parameters 'fi', 'format_string', 'm' and 'destroy'
 * will be passed to the kedr_fsim_set_indicator(),
 * when this indicator will be registered for some simulation point
 * via kedr_fsim_set_indicator_by_name().
 *
 * Also, this indicator will be automatically unregistered,
 * when module 'm' will be unloaded.
 * (But one may safely call kedr_fsim_indicator_function_unregister before module go to unload).
 *
 * 'init_state' - function which will be called, when indicator will be registered 
 * for some simulation point via kedr_fsim_set_indicator_by_name().
 * Result('indicator_state' parameter) of this function will be passed
 * to the kedr_fsim_set_indicator() function align with other parameters.
 *
 * Return 0 on success.
 * If 'indicator_name' already used as name of other indicator, return not 0.
 */

int kedr_fsim_indicator_function_register(const char* indicator_name,
	kedr_fsim_fault_indicator fi, const char* format_string,
	struct module* m,
	kedr_fsim_init_indicator_state init_state,
	kedr_fsim_destroy_indicator_state destroy);

/*
 * Unregister indicator function, making indicator_name free for use
 * in kedr_fsim_indicator_function_register().
 *
 * NOTE: this unregistering DOES NOT reset any indicators, which was
 * already set for simulation points. So, even after unregistering,
 * 'fi', 'init_state' and 'destroy' functions may be called for 
 * different simulation points, until module 'm' will be unloaded.
 *
 * This unregistering only break out relation between 'indicator_name',
 * and its current meaning - incapsulating indicator parameters.
 */

void kedr_fsim_indicator_function_unregister(const char* indicator_name);

/*
 * Set indicator function for simulation point. 
 * 
 * Function behave similar to kedr_fsim_set_indicator(),
 * but instead of indicator function it accept name of this indicator
 * (this name should be binding with indicator function via
 * kedr_fsim_indicator_function_register).
 * 
 * 'params' and 'params_len' are passed to the 'init_state' function
 * (see description of kedr_fsim_indicator_function_register).
 * 
 * 'indicator_name' NULL or "" means to clear indicator for the point.
 */

int kedr_fsim_set_indicator_by_name(const char* point_name,
	const char* indicator_name, const void* params, size_t params_len);

#else /* __KERNEL__*/

#include <unistd.h> /*size_t, NULL*/

/*
 * Set indicator for particular simulation point.
 * (make call of kedr_fsim_set_indicator_by_name() kernel function)
 * 
 * 'point_name' is name of simulation point,
 * which should previously registered with kedr_fsim_point_register.
 
 * 'indicator_name' - name of indicator, which should previously registered
 * with kedr_fsim_indicator_function_register() in the kernel.
 *
 * 'indicator_name' NULL or "\0" value means to reset indicator for this point
 * (so, simulation for this point will always return 0, success).
 
 * 'params' should be array of bytes with 'params_len' size.
 * Its content will be transmitted to the kernel call
 * kedr_fsim_set_indicator_by_name().
 * 
 * On success returns 0.
 * If 'point_name' or 'indicator_name' don't registered, or 'params' array
 * has incorrect format for given indicator, returns not 0.
 */

int kedr_fsim_set_indicator(const char* point_name,
	const char* indicator_name, void* params, size_t params_len);

/*
 * Helper functions for simple indicators.
 */

/*
 * Always simulate failure.
 */

static inline int
kedr_fsim_set_indicator_always_fault(const char* point_name)
{
	return kedr_fsim_set_indicator(point_name,
		"always_fault", NULL, 0);
}

/*
 * Simulate failure always after module has initialized.
 */

static inline int
kedr_fsim_set_indicator_always_fault_after_init(const char* point_name)
{
	return kedr_fsim_set_indicator(point_name,
		"always_fault_after_init", NULL, 0);
}

/*
 * Take size_t parameter (size_limit) when is set
 * for particular simulation point, and pointer to
 * size_t parameter (size) when need to simulate.
 * 
 * Simulate failure if size > size_limit.
 */

static inline int
set_indicator_fault_if_size_greater(const char* point_name,
	size_t size_limit)
{
	return kedr_fsim_set_indicator(point_name,
		"fault_if_size_greater",
		&size_limit, sizeof(size_limit));
}

#endif /* __KERNEL */
#endif /* FSIM_INDICATOR_MANAGER_H */
