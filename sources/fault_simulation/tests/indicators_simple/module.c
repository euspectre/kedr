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

#include <kedr/fault_simulation/fault_simulation.h>

const char* indicator_name_never = "never";
const char* indicator_name_always = "always";

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static int simulate_never(void* indicator_state, void* point_data){return 0;}
static int simulate_always(void* indicator_state, void* point_data){return 1;}

struct kedr_simulation_indicator* indicator_never;
struct kedr_simulation_indicator* indicator_always;

static int __init
this_module_init(void)
{
    indicator_never = kedr_fsim_indicator_register(indicator_name_never,
        simulate_never, "", NULL, NULL);
    if(indicator_never == NULL)
    {
        printk(KERN_ERR "Cannot register indicator with 'never' scenario.\n");
        return -EINVAL;
    }

    indicator_always = kedr_fsim_indicator_register(indicator_name_always,
        simulate_always, "", NULL, NULL);
    if(indicator_always == NULL)
    {
        printk(KERN_ERR "Cannot register indicator with 'always' scenario.\n");
        kedr_fsim_indicator_unregister(indicator_never);
        return -EINVAL;
    }
    return 0;
}

static void
this_module_exit(void)
{
	kedr_fsim_indicator_unregister(indicator_never);
	kedr_fsim_indicator_unregister(indicator_always);
	return;
}

module_init(this_module_init);
module_exit(this_module_exit);