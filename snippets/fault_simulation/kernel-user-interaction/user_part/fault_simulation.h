#ifndef FAULT_SIMULATION_H
#define FAULT_SIMULATION_H

#include <unistd.h> /*size_t, NULL*/

/*
 * API for setting different types of fault simulation
 * for different simulation points.
 */

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

static inline int
kedr_fsim_set_indicator_always_fault(const char* point_name)
{
	return kedr_fsim_set_indicator(point_name,
		"always_fault", NULL, 0);
}

static inline int
kedr_fsim_set_indicator_always_fault_after_init(const char* point_name)
{
	return kedr_fsim_set_indicator(point_name,
		"always_fault_after_init", NULL, 0);
}

static inline int set_indicator_fault_if_size_greater(const char* point_name,
	size_t size_limit)
{
	return kedr_fsim_set_indicator(point_name,
		"fault_if_size_greater",
		&size_limit, sizeof(size_limit));
}

#endif /* FAULT_SIMULATION_H */
