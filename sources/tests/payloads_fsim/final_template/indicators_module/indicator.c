/*
 * Fault simulation indicator which accepts no parameters
 * and return value which is set from user-space.
 */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Copyright (C) 2010-2012, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>

#include <kedr/fault_simulation/fault_simulation.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>     /* kmalloc() */


MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

/********************* Constant indicator *****************************/

/*
 * Name of the indicator (it is not the name of the module!),
 * using which one may set indicator for the particular fault simulation point.
 */

const char* indicator_constant_name = "constant";
const char* indicator_constant_format_string = "";

/*
 * Describe parameters, which may be unique for every instance of indicator.
 *
 * These parameters are packed into structure, which is passed to the indicator at simulate stage
 * and to the other callbacks.
 */

struct indicator_constant_state
{
    /*
     * Result returned by the indicator.
     */
    int result;
};

static int indicator_constant_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory)
{
    struct indicator_constant_state* state;
    unsigned long indicator_result;
    // Read 'params' and convert its to the value of the result
    if((params == NULL) || (*params == '\0'))
    {
        //Without parameters, set result to the default value
        indicator_result = 0;
    }
    else
    {
        int error = strict_strtoul(params, 10, &indicator_result);
        if(error)
        {
            pr_err("Cannot convert '%s' to integer result of the scenario.", params);
            return error;
        }
    }
    // Allocate state instance.
    state = kmalloc(sizeof(*state), GFP_KERNEL);
    if(state == NULL)
    {
        pr_err("Cannot allocate indicator state instance.");
        return -ENOMEM;
    }
    // Set state variables
    state->result = (int)indicator_result;
    // Store indicator state
    *indicator_state = state;
    return 0;
}

static int indicator_constant_simulate(void* indicator_state, void* user_data)
{
    struct indicator_constant_state* state = indicator_state;
    return state->result;
}

static void indicator_constant_instance_destroy(void* indicator_state)
{
    kfree(indicator_state);
}

/******************** Indicator 'greater than' ************************/
#define DEFAULT_BOUNDARY 10

/*
 * Name of the indicator (it is not the name of the module!),
 * using which one may set indicator for the particular fault simulation point.
 */

const char* indicator_greater_than_name = "greater_than";

const char* indicator_greater_than_format_string = "int";

struct indicator_greater_than_data
{
    int value;
};

/*
 * Describe parameters, which may be unique for every instance of indicator.
 *
 * These parameters are packed into structure, which is passed to the indicator at simulate stage
 * and to the other callbacks.
 */

struct indicator_greater_than_state
{
    /*
     * Value to which parameter of the indicator is compared.
     */
    int boundary;
};

static int indicator_greater_than_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory)
{
    struct indicator_greater_than_state* state;
    unsigned long indicator_boundary;
    // Read 'params' and convert its to the value of the boundary
    if((params == NULL) || (*params == '\0'))
    {
        //Without parameters, set boundary to the default value
        indicator_boundary = DEFAULT_BOUNDARY;
    }
    else
    {
        int error = strict_strtoul(params, 10, &indicator_boundary);
        if(error)
        {
            pr_err("Cannot convert '%s' to integer boundary of the scenario.", params);
            return error;
        }
    }
    // Allocate state instance.
    state = kmalloc(sizeof(*state), GFP_KERNEL);
    if(state == NULL)
    {
        pr_err("Cannot allocate indicator state instance.");
        return -ENOMEM;
    }
    // Set state variables
    state->boundary = (int)indicator_boundary;
    // Store indicator state
    *indicator_state = state;
    return 0;
}

static int indicator_greater_than_simulate(void* indicator_state, void* user_data)
{
    struct indicator_greater_than_state* state = indicator_state;
    struct indicator_greater_than_data* data_real = user_data;
    return data_real->value > state->boundary? 1 : 0;
}

static void indicator_greater_than_instance_destroy(void* indicator_state)
{
    kfree(indicator_state);
}

/******************** Global definitions ******************************/
/* 
 * References to the indicators which will be registered.
 */
struct kedr_simulation_indicator* indicator_constant;
struct kedr_simulation_indicator* indicator_greater_than;

static int __init
indicators_init(void)
{
    // Register indicators
    indicator_constant = kedr_fsim_indicator_register(
        indicator_constant_name,
        indicator_constant_simulate,
        indicator_constant_format_string, 
        indicator_constant_instance_init,
        indicator_constant_instance_destroy);

    if(indicator_constant == NULL)
    {
        pr_err("Cannot register indicator '%s'.\n", indicator_constant_name);
        return -EINVAL;
    }

    indicator_greater_than = kedr_fsim_indicator_register(
        indicator_greater_than_name,
        indicator_greater_than_simulate,
        indicator_greater_than_format_string, 
        indicator_greater_than_instance_init,
        indicator_greater_than_instance_destroy);

    if(indicator_greater_than == NULL)
    {
        pr_err("Cannot register indicator '%s'.\n", indicator_greater_than_name);
        kedr_fsim_indicator_unregister(indicator_constant);
        return -EINVAL;
    }
    return 0;
}

static void
indicators_exit(void)
{
	// Deregistration of the indicators
	kedr_fsim_indicator_unregister(indicator_greater_than);
    kedr_fsim_indicator_unregister(indicator_constant);
	return;
}

module_init(indicators_init);
module_exit(indicators_exit);
