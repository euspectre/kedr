/*
 * Example of using fault simulation API for create payload
 * modules with fault simulation capability in replacement functions.
 */

/* ========================================================================
 * Copyright (C) 2010-2011, Institute for System Programming 
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

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>  /* kmalloc replacement*/

#include <kedr/fault_simulation/fault_simulation.h>
#include <kedr/core/kedr.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

//********* Replacement function for __kmalloc ***********************

/*
 * Name of the fault simulation point, which will used in __kmalloc replacement.
 *
 * Fault simulation indicator, which will be set for this point, will affect
 * on the execution of the replacement function.
 *
 * Use name of the target function to make it self-describing.
 */

static const char* point_name___kmalloc = "__kmalloc";

/*
 * Declare parameters, which will be passed to fault simulation scenario for
 * __kmalloc replacement function.
 */

struct point_data___kmalloc
{
    size_t size;
    gfp_t flags;
};

/*
 * String, which describe parameters, which fault simulation point passes to
 * fault simulation scenario.
 *
 * This string is used to prevent setting for this point of
 * fault simulation indicator, which require more parameters than point provides.
 *
 * It should be comma-separated list of parameters type in same order, as them
 * declared in the structure.
 */
const char* point_format_string___kmalloc = "size_t,gfp_t";

// Fault simulation point object, for use in simulation.
struct kedr_simulation_point* point___kmalloc;

//
static void* repl___kmalloc(size_t size, gfp_t flags,
    struct kedr_function_call_info* call_info)
{
    int need_simulate;
    // Form data for simulate scenario
    struct point_data___kmalloc point_data = 
    {
        .size = size,
        .flags = flags
    };
    // Call simulate for fault simulation point
    need_simulate = kedr_fsim_point_simulate(point___kmalloc, &point_data);
    
    if(need_simulate)
    {
        // Simulate failure
        return NULL;
    }
    else
    {
        // Simple call target function(without failure simulation)
        return __kmalloc(size, flags);
    }
}

// There are may be other replacement functions...


// Form data for payload registration
struct kedr_replace_pair replace_pairs[] =
{
    {
        .orig = (void*)&__kmalloc,
        .replace = (void*)&repl___kmalloc
    },
    /* terminating element*/
    {
        .orig = NULL
    }
};

struct kedr_payload sample_fsim_payload =
{
    .mod = THIS_MODULE,

    .replace_pairs = replace_pairs,

    // Do not catch signals on target load/unload
    .target_load_callback   = NULL,
    .target_unload_callback = NULL
};

/*
 * This functions are defined in functions_support.c source file,
 * which is generated using "functions_support.c" templates set
 * from definition of the original functions for intercept.
 */
extern int functions_support_register(void);
extern void functions_support_unregister(void);

static int __init
sample_fsim_payload_init(void)
{
    //Register fault simulation point
    point___kmalloc = kedr_fsim_point_register(point_name___kmalloc,
        point_format_string___kmalloc);
    
    if(point___kmalloc == NULL)
    {
        pr_err("Cannot register simulation point for __kmalloc replacement.\n");
        return -EINVAL;
    }
    // Register KEDR support for intercepted functions
    if(functions_support_register())
    {
        pr_err("Failed to register support for intercepted functions.");
        kedr_fsim_point_unregister(point___kmalloc);
        return -EINVAL;
    }
    // Register payload
    if(kedr_payload_register(&sample_fsim_payload))
    {
        pr_err("Cannot register sample payload.\n");
        kedr_fsim_point_unregister(point___kmalloc);
        functions_support_unregister();
        return -EINVAL;
    }

    return 0;
}

static void
sample_fsim_payload_exit(void)
{
	kedr_payload_unregister(&sample_fsim_payload);
    functions_support_unregister();
	kedr_fsim_point_unregister(point___kmalloc);
	return;
}

module_init(sample_fsim_payload_init);
module_exit(sample_fsim_payload_exit);