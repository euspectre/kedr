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

#include <linux/kernel.h>	/* printk() */

#include <kedr/fault_simulation/fault_simulation.h>

const char* read_indicator_name = "indicator_for_read";
const char* write_indicator_name = "indicator_for_write";

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static int read_indicator_instances = 0;
static int write_indicator_instances = 0;

module_param(read_indicator_instances, int, S_IRUGO);
module_param(write_indicator_instances, int, S_IRUGO);

static int indicator_return_9(void* indicator_state, void* user_data)
{
    (void)indicator_state;
    (void)user_data;
    
    return 9;
}

static int indicator_return_1arg(void* indicator_state, void* user_data)
{
    printk(KERN_INFO "Indicator was called.\n");
    (void)indicator_state;
    
    return (int)*((size_t*)user_data);
}

static void read_indicator_instance_destroy(void* indicator_state)
{
    (void)indicator_state;
    read_indicator_instances--;
}
static void write_indicator_instance_destroy(void* indicator_state)
{
    (void)indicator_state;
    write_indicator_instances--;
}

static int write_indicator_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory)
{
    *indicator_state = NULL;
    (void)control_directory;
    
    write_indicator_instances++;
    
    return 0;
}

static int read_indicator_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory)
{
    *indicator_state = NULL;
    (void)control_directory;
    
    read_indicator_instances++;
    
    return 0;
}


struct kedr_simulation_indicator* indicator_read;
struct kedr_simulation_indicator* indicator_write;

static int __init
module_b_init(void)
{
    indicator_read = kedr_fsim_indicator_register(read_indicator_name,
        indicator_return_9, "",
        read_indicator_instance_init,
        read_indicator_instance_destroy);
    if(indicator_read == NULL)
    {
        printk(KERN_ERR "Cannot register read indicator.\n");
        return -1;
    }

    indicator_write = kedr_fsim_indicator_register(write_indicator_name,
        indicator_return_1arg, "size_t",
        write_indicator_instance_init,
        write_indicator_instance_destroy);
    if(indicator_write == NULL)
    {
        printk(KERN_ERR "Cannot register write indicator.\n");
        kedr_fsim_indicator_unregister(indicator_read);
        return -1;
    }

    return 0;
}

static void
module_b_exit(void)
{
	kedr_fsim_indicator_unregister(indicator_read);
	kedr_fsim_indicator_unregister(indicator_write);
	BUG_ON(read_indicator_instances != 0);
    BUG_ON(write_indicator_instances != 0);
	return;
}

module_init(module_b_init);
module_exit(module_b_exit);