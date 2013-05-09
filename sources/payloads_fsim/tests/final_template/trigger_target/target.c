/*
 * Module which allows to trigger __kmalloc, krealloc, __krealloc
 * and kasprintf calls.
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

#include <linux/kernel.h>	/* printk() */

#include <linux/module.h>

#include <linux/slab.h> /*kmalloc*/
    
#include <linux/debugfs.h> /* control files will be create on debugfs*/

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

/*
 * Directory which will contatins control file.
 */
struct dentry* module_dir = NULL;

struct dentry* __kmalloc_trigger_file = NULL;
struct dentry* krealloc_trigger_file = NULL;
struct dentry* __krealloc_trigger_file = NULL;
struct dentry* kasprintf_trigger_file = NULL;

static void remove_dentries(void);

///////////////////////////File operations///////////////////////
static ssize_t
__kmalloc_trigger_file_write(struct file* filp,
    const char __user *buf, size_t count, loff_t* f_pos)
{
    void* p = __kmalloc(count, GFP_KERNEL);
    if(p == NULL) return -ENOMEM;
    kfree(p);
    return count;
}

static struct file_operations __kmalloc_trigger_file_operations = {
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

static ssize_t
kasprintf_trigger_file_write(struct file* filp,
    const char __user *buf, size_t count, loff_t* f_pos)
{
    char* str;
    char* str1, *str2;
    
    size_t str1_len = count / 2;
    size_t str2_len = (count -1) / 2;
    /* Now str1_len + str2_len = count - 1 */
    
    str1 = kmalloc(str1_len + 1, GFP_KERNEL);
    if(str1 == NULL) return -ENOMEM;
    
    str2 = kmalloc(str2_len + 1, GFP_KERNEL);
    if(str2 == NULL)
    {
        kfree(str1);
        return -ENOMEM;
    }
    
    /* Fill str1 and str2 with some values*/
    memset(str1, '1', str1_len);
    str1[str1_len] = '\0';
    
    memset(str2, '2', str2_len);
    str2[str2_len] = '\0';
    
    // Allocated size of str will be str1_len + str2_len + 1 = count.
    str = kasprintf(GFP_KERNEL, "%s%s", str1, str2);
    
    if(str == NULL)
    {
        kfree(str1);
        kfree(str2);

        return -ENOMEM;
    }
    //Check that result is correct. So replacement function do right things.
    BUG_ON(memcmp(str, str1, str1_len));
    BUG_ON(strcmp(str + str1_len, str2));
    
    kfree(str1);
    kfree(str2);
    kfree(str);
    
    return count;
}

static struct file_operations kasprintf_trigger_file_operations = {
    .owner = THIS_MODULE,
    .write = kasprintf_trigger_file_write
};

///
static int __init
this_module_init(void)
{
    int error = -EINVAL;
    
    module_dir = debugfs_create_dir("function_triggers", NULL);
    if(module_dir == NULL)
    {
        pr_err("Cannot create root directory in debugfs for module.");
        goto err;
    }

#define CREATE_TRIGGER_FILE(function) \
    function##_trigger_file = debugfs_create_file(#function, \
        S_IWUSR | S_IWGRP, \
        module_dir, \
        NULL, \
        &function##_trigger_file_operations \
        ); \
    if(function##_trigger_file == NULL) \
    { \
        pr_err("Cannot create trigger file for " #function "."); \
        goto err; \
    }

    CREATE_TRIGGER_FILE(__kmalloc)
    CREATE_TRIGGER_FILE(__krealloc)
    CREATE_TRIGGER_FILE(krealloc)
    CREATE_TRIGGER_FILE(kasprintf)

#undef CREATE_TRIGGER_FILE
    
    return 0;

err:
    remove_dentries();
    return error;
}

static void
this_module_exit(void)
{
    remove_dentries();
}

static void remove_dentry(struct dentry* dentry)
{
    if(dentry) debugfs_remove(dentry);
}

void remove_dentries(void)
{
    if(module_dir)
    {
        remove_dentry(kasprintf_trigger_file);
        remove_dentry(krealloc_trigger_file);
        remove_dentry(__krealloc_trigger_file);
        remove_dentry(__kmalloc_trigger_file);

        debugfs_remove(module_dir);
    }
}

module_init(this_module_init);
module_exit(this_module_exit);
