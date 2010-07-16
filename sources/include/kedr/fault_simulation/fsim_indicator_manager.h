#ifndef FSIM_INDICATOR_MANAGER_H
#define FSIM_INDICATOR_MANAGER_H

/*
 * API for managing indicator functions.
 */

#ifdef __KERNEL__

#include <kedr/fault_simulation/fsim_base.h>

/*
 * Type of function which initialize indicator state according
 * to parameters. Parameters are represented by array of bytes
 * ('params') of some length (params_len).
 * 
 * This parameter representation is suitable for to be transfered
 * as message via sockets.
 * 
 * On success, function should return 0.
 * 
 * If format of parameters is invalid or data parameter for
 * indicator cannot be created for others reasons,
 * function should return not 0.
 */

typedef int (*kedr_fsim_init_indicator_state)(const void* params,
	size_t params_len, void** indicator_state);

/*
 * Bind indicator_name with particular indicator functions.
 */

int kedr_fsim_indicator_function_register(const char* indicator_name,
	kedr_fsim_fault_indicator fi, const char* format_string,
	struct module* m,
	kedr_fsim_init_indicator_state init_state,
	kedr_fsim_destroy_indicator_state destroy_state);

/*
 *  Unregister indicator function.
 */

void kedr_fsim_indicator_function_unregister(const char* name);

/*
 * Set indicator function for simulation point. 
 * 
 * Function behaviour is similar to kedr_fsim_set_indicator,
 * but instead of indicator function it accept name of this indicator
 * (this name should be binding with indicator function via
 * kedr_fsim_indicator_function_register).
 * Also, others parameters of indicator are passed via array of bytes.
 * 
 * 'indicator_name' NULL or "" means to clear indicator for the point.
 */

int kedr_fsim_set_indicator_by_name(const char* point_name,
	const char* indicator_name, const void* params, size_t params_len);

#else /* __KERNEL__*/

#include <unistd.h> /*size_t, NULL*/

/*
 * Set indicator for particular simulation point.
 * 
 * 'params' array of bytes is used as additional parameter(s)
 * for indicator function. In common cases params may be NULL
 * (params_len should be 0 in that case).
 * 
 * On success returns 0.
 * If point_name or indicator_name don't exist, or params array
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
