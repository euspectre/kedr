/*
 * Module which allows to trigger my_kasprintf with variable number of arguments.
 */

/* ========================================================================
 * Copyright (C) 2012, 2013 KEDR development team
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

#include <linux/kernel.h>	/* printk() */

#include <linux/module.h>

#include <linux/slab.h> /*kmalloc*/
    
#include <linux/debugfs.h> /* control files will be create on debugfs*/

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

/*
 * Directory which will contatins control file.
 */
struct dentry* module_dir;

struct dentry* my_kasprintf_trigger_file;


///////////////////////////File operations///////////////////////
static ssize_t
my_kasprintf_trigger_file_write(struct file* filp,
    const char __user *buf, size_t count, loff_t* f_pos)
{
    void* p = __kmalloc(count, GFP_KERNEL);
    if(p == NULL) return -ENOMEM;
    kfree(p);
    return count;
}

static struct file_operations my_ksprintf_trigger_file_operations = {
    .owner = THIS_MODULE,
    .write = __kmalloc_trigger_file_write
};


static ssize_t
__krealloc_trigger_file_write(struct file* filp,
    const char __user *buf, size_t count, loff_t* f_pos)
{
    void* p = __krealloc(NULL, count, GFP_KERNEL);
    if(p == NULL) return -ENOMEM;
    kfree(p);
    return count;
}

static struct file_operations __krealloc_trigger_file_operations = {
    .owner = THIS_MODULE,
    .write = __krealloc_trigger_file_write
};

static ssize_t
krealloc_trigger_file_write(struct file* filp,
    const char __user *buf, size_t count, loff_t* f_pos)
{
    void* p = krealloc(NULL, count, GFP_KERNEL);
    if(p == NULL) return -ENOMEM;
    kfree(p);
    return count;
}

static struct file_operations krealloc_trigger_file_operations = {
    .owner = THIS_MODULE,
    .write = krealloc_trigger_file_write
};

///
static int __init
this_module_init(void)
{
    module_dir = debugfs_create_dir("function_triggers", NULL);
    if(module_dir == NULL)
    {
        pr_err("Cannot create root directory in debugfs for module.");
        return -EINVAL;
    }

    __kmalloc_trigger_file = debugfs_create_file("__kmalloc",
        S_IWUSR | S_IWGRP,
        module_dir,
        NULL,
        &__kmalloc_trigger_file_operations
        );
    if(__kmalloc_trigger_file == NULL)
    {
        pr_err("Cannot create trigger file for __kmalloc.");
        debugfs_remove(module_dir);
        return -EINVAL;
    }

    __krealloc_trigger_file = debugfs_create_file("__krealloc",
        S_IWUSR | S_IWGRP,
        module_dir,
        NULL,
        &__krealloc_trigger_file_operations
        );
    if(__krealloc_trigger_file == NULL)
    {
        pr_err("Cannot create trigger file for __krealloc.");
        debugfs_remove(__kmalloc_trigger_file);
        debugfs_remove(module_dir);
        return -EINVAL;
    }

    krealloc_trigger_file = debugfs_create_file("krealloc",
        S_IWUSR | S_IWGRP,
        module_dir,
        NULL,
        &krealloc_trigger_file_operations
        );
    if(krealloc_trigger_file == NULL)
    {
        pr_err("Cannot create trigger file for krealloc.");
        debugfs_remove(krealloc_trigger_file);
        debugfs_remove(__kmalloc_trigger_file);
        debugfs_remove(module_dir);
        return -EINVAL;
    }

    return 0;
}

static void
this_module_exit(void)
{
    debugfs_remove(krealloc_trigger_file);
    debugfs_remove(__krealloc_trigger_file);
    debugfs_remove(__kmalloc_trigger_file);
    debugfs_remove(module_dir);
}
module_init(this_module_init);
module_exit(this_module_exit);
