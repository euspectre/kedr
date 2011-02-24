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

#include <linux/kernel.h>	/* printk() */

#include <linux/module.h>

#include <linux/slab.h> /*kmalloc*/
    
#include <linux/debugfs.h> /* control file will be create on debugfs*/

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

/*
 * Directory which will contatins control file.
 */
struct dentry* module_dir;

struct dentry* control_file;

///////////////////////////File operations///////////////////////
ssize_t control_file_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)
{
    void* p = __kmalloc(10, GFP_KERNEL);
    if(p == NULL) return -ENOMEM;
    kfree(p);
    return count;
}

static struct file_operations control_file_operations = {
    .owner = THIS_MODULE,
    .write = control_file_write
};

///
static int __init
this_module_init(void)
{
    module_dir = debugfs_create_dir("target_caller_address", NULL);
    if(module_dir == NULL)
    {
        pr_err("Cannot create root directory in debugfs for module.");
        return -EINVAL;
    }
    control_file = debugfs_create_file("control",
        S_IWUSR | S_IWGRP,
        module_dir,
        NULL,
        &control_file_operations
        );
    if(control_file == NULL)
    {
        pr_err("Cannot create control file.");
        debugfs_remove(module_dir);
        return -EINVAL;
    }
    return 0;
}

static void
this_module_exit(void)
{
    debugfs_remove(control_file);
    debugfs_remove(module_dir);
}
module_init(this_module_init);
module_exit(this_module_exit);
