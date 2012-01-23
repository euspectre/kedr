/* ========================================================================
 * Copyright (C) 2012, KEDR development team
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

#include <linux/kernel.h>	/* printk() */

#include <linux/module.h>
    
#include <linux/debugfs.h> /* control file will be created on debugfs*/

#include <kedr/fault_simulation/fault_simulation.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

/*
 * Directory which will contatins simulate control file.
 */
static struct dentry* module_dir;

static struct dentry* simulate_file;
static struct kedr_simulation_point* point;

///////////////////////////File operations///////////////////////
static ssize_t simulate_file_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)
{
    return kedr_fsim_point_simulate(point, NULL) ? -EINVAL : count;
}

static struct file_operations simulate_file_operations = {
    .owner = THIS_MODULE,
    .write = simulate_file_write
};

///
static int __init
this_module_init(void)
{
    point = kedr_fsim_point_register("common", "");
    if(point == NULL)
    {
        pr_err("Cannot register simulation point.");
        return -EINVAL;
    }
    
    module_dir = debugfs_create_dir("kedr_indicator_internal_test_module", NULL);
    if(module_dir == NULL)
    {
        pr_err("Cannot create root directory in debugfs for module.");
        kedr_fsim_point_unregister(point);
        return -EINVAL;
    }
    simulate_file = debugfs_create_file("simulate",
        S_IWUSR | S_IWGRP,
        module_dir,
        NULL,
        &simulate_file_operations
        );
    if(simulate_file == NULL)
    {
        pr_err("Cannot create control file.");
        debugfs_remove(module_dir);
        kedr_fsim_point_unregister(point);
        return -EINVAL;
    }
    return 0;
}

static void
this_module_exit(void)
{
    debugfs_remove(simulate_file);
    debugfs_remove(module_dir);
    kedr_fsim_point_unregister(point);
}
module_init(this_module_init);
module_exit(this_module_exit);
