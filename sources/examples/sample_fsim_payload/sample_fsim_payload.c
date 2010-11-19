/*
 * Example of using fault simulation API for create payload
 * modules with fault simulation capability in replacement functions.
 */

/* ========================================================================
 * Copyright (C) 2010, Institute for System Programming 
 *                     of the Russian Academy of Sciences (ISPRAS)
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
#include <kedr/base/common.h>

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
static void* repl___kmalloc(size_t size, gfp_t flags)
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
static void* original_addresses[] = 
{
    __kmalloc,
};

static void* replacement_addresses[] = 
{
    repl___kmalloc,
};

struct kedr_payload sample_fsim_payload =
{
    .mod = THIS_MODULE,
    .repl_table =
    {
        .orig_addrs = original_addresses,
        .repl_addrs = replacement_addresses,
        .num_addrs = ARRAY_SIZE(original_addresses)
    },
    // Do not catch signals on target load/unload
    .target_load_callback   = NULL,
    .target_unload_callback = NULL
};

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
    // Register payload
    if(kedr_payload_register(&sample_fsim_payload))
    {
        pr_err("Cannot register sample payload.\n");
        kedr_fsim_point_unregister(point___kmalloc);
        return -EINVAL;
    }

    return 0;
}

static void
sample_fsim_payload_exit(void)
{
	kedr_payload_unregister(&sample_fsim_payload);
	kedr_fsim_point_unregister(point___kmalloc);
	return;
}

module_init(sample_fsim_payload_init);
module_exit(sample_fsim_payload_exit);