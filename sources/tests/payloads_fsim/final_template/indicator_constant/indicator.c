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

/* 
 * Reference to the indicator which will be registered.
 */
struct kedr_simulation_indicator* indicator_constant;

static int __init
indicator_constant_init(void)
{
    // Registering of the indicator.
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
    return 0;
}

static void
indicator_constant_exit(void)
{
	// Deregistration of the indicator
	kedr_fsim_indicator_unregister(indicator_constant);
	return;
}

module_init(indicator_constant_init);
module_exit(indicator_constant_exit);
