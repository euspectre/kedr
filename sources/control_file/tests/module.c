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

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/module.h>
    
#include <linux/mutex.h>

#include <linux/debugfs.h> /* control file will be create on debugfs*/

#include <linux/string.h> /* strlen, strcpy */

#include <kedr/control_file/control_file.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

/*
 * Characteristic, exposed into user space.
 *
 * In our case - string which contain name of current object.
 *
 * This should be non-empty string.
 * (Correct processing wrintting of empty string into file is non-trivial task,
 * because it may be just opening file with O_TRUNC flag).
 */
static char* object_name = NULL;
// Protect 'object_name' from concurrent reads and writes.
static DEFINE_MUTEX(object_name_mutex);

/*
 * Value of characteristic(object name) which is used as default - 
 * e.g. value of characteristic before the first setting,
 * 
 * It may meen absence of something. 
 */
static const char* object_name_default = "noname";

/*
 * Directory which will contatins our files(in our case - 1 file).
 */
struct dentry* module_dir;

/*
 * Control file.
 * 
 * In our case, reflect 'object_name' variable.
 */
struct dentry* object_name_file;

///////////////////Auxiliary functions////////////////////////////////

/*
 * Return current characteristic of module.
 *
 * In our case - name of object.
 *
 * Should be executed under mutes taken.
 */

static const char* get_object_name_internal(void);

/*
 * Perform 'reaction' of module on user-space impact.
 *
 * In our case - set object name.
 *
 * Should be executed under mutes taken.
 *
 * Return 0 on success, negative error code otherwise.
 */
static int set_object_name_internal(const char* new_name);



///////////////////////////File operations///////////////////////
static char* object_name_get_str(struct inode* inode);
static int object_name_set_str(const char* str, struct inode* inode);

CONTROL_FILE_OPS(object_name_file_operations, object_name_get_str, object_name_set_str);
/////////////////////////////////////////////////////////////////////////////

static int __init
this_module_init(void)
{
    object_name = kmalloc(strlen(object_name_default) + 1, GFP_KERNEL);
    if(object_name == NULL)
    {
        pr_err("Cannot allocate name.");
        return -1;
    }
    strcpy(object_name, object_name_default);

    module_dir = debugfs_create_dir("kedr_control_file_test_module", NULL);
    if(module_dir == NULL)
    {
        pr_err("Cannot create root directory in debugfs for service.");
        kfree(object_name);
        return -1;
    }
    object_name_file = debugfs_create_file("current_name",
        S_IRUGO | S_IWUSR | S_IWGRP,
        module_dir,
        NULL,
        &object_name_file_operations
        );
    if(object_name_file == NULL)
    {
        pr_err("Cannot create control file.");
        debugfs_remove(module_dir);
        kfree(object_name);
        return -1;
    }
    
    return 0;
}

static void
this_module_exit(void)
{
    debugfs_remove(object_name_file);
    debugfs_remove(module_dir);
    kfree(object_name);
}
module_init(this_module_init);
module_exit(this_module_exit);

////////////////////Implementation of auxiliary functions////////////////////////////////

char* object_name_get_str(struct inode* inode)
{
    char* result;
    if(mutex_lock_killable(&object_name_mutex))
    {
        pr_debug("was killed.");
        return NULL;
    }
    result = kstrdup(get_object_name_internal(), GFP_KERNEL);
    mutex_unlock(&object_name_mutex);
    return result;
}
int object_name_set_str(const char* str, struct inode* inode)
{
    int result;
    if(mutex_lock_killable(&object_name_mutex))
    {
        pr_debug("operation was killed.");
        return -EAGAIN;
    }
    result = set_object_name_internal(str);
    mutex_unlock(&object_name_mutex);
    return result;
}

/*
 * Return current characteristic of module.
 *
 * In our case - name of object.
 * Should be executed under mutes taken.
 */

const char* get_object_name_internal(void)
{
    return object_name;
}

/*
 * Perform 'reaction' of module on user-space impact.
 *
 * In our case - set object_name.
 * Should be executed under mutes taken.
 *
 * Return 0 on success, negative error code otherwise.
 */
int set_object_name_internal(const char* new_name)
{
    char* new_name_instance;
    
    if(*new_name == '\0') return -EINVAL;//name should be non-empty
    
    new_name_instance = kmalloc(strlen(new_name) + 1, GFP_KERNEL);
    if(new_name_instance == NULL) return -ENOMEM;
    strcpy(new_name_instance, new_name);
        
    kfree(object_name);
    object_name = new_name_instance;

    pr_debug("New name of object is '%s'.", object_name);
    return 0;
}

