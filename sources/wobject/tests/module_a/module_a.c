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
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/delay.h>    /* ssleep() */

#include <linux/fs.h>		/* everything... */
#include <linux/cdev.h>

#include <linux/mutex.h> /* mutexes */

#include <kedr/wobject/wobject.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static wobj_t obj;
static wobj_weak_ref_t weak_ref;
//whether object is accessible
static int obj_live = 0;
module_param(obj_live, int, S_IRUGO);

static void finalize(wobj_t* obj)
{
    obj_live = 0;
}
//whether weak reference is accessible
static int weak_ref_live = 0;
module_param(weak_ref_live, int, S_IRUGO);

//whether callback was called(reseted after each weak_ref)
static int callback_was_called = 0;
module_param(callback_was_called, int, S_IRUGO);

static void weak_ref_callback(wobj_weak_ref_t* weak_ref)
{
    weak_ref_live = 0;
    callback_was_called = 1;
}

enum command_read_counts
{
    command_init_obj = 1,// wobj_init(&obj)
    command_ref = 2,// wobj_ref(&obj)
    command_unref = 3,// wobj_unref(&obj)
    command_unref_final = 4, //wobj_unref_final(&obj)
};

enum command_write_counts
{
    command_weak_ref = 1,// wobj_weak_ref_init(&weak_ref, &obj, callback)
    command_weak_ref_ref = 2,// wobj_weak_ref_get(&weak_ref)
    command_weak_ref_clear = 3,// wobj_weak_ref_clear(&weak_ref)
};


const char* device_name = "kedr_test_device";

//Device
int module_major = 0;
int module_minor = 0;

struct cdev module_device;

ssize_t 
module_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos);

ssize_t
module_write(struct file *filp, const char __user *buf, size_t count, 
	loff_t *f_pos);

struct file_operations module_fops = {
    .owner = THIS_MODULE,
    .read =     module_read,
    .write =    module_write,
};

static int init_device(void)
{
    dev_t dev = 0;
    if(alloc_chrdev_region(&dev, module_minor, 1, device_name) < 0)
    {
        printk(KERN_ERR "Cannot allocate device number for test");
        return 1;
    }
	module_major = MAJOR(dev);
    
   	cdev_init(&module_device, &module_fops);
	module_device.owner = THIS_MODULE;
	module_device.ops = &module_fops;
	
	if(cdev_add(&module_device, dev, 1))
    {
        printk(KERN_ERR "Cannot add device for test");
        unregister_chrdev_region(dev, 1);
        return 1;
    }
	return 0;
}

static void delete_device(void)
{
    dev_t dev = MKDEV(module_major, module_minor);
    cdev_del(&module_device);
    unregister_chrdev_region(dev, 1);
}

static int __init
module_a_init(void)
{
    init_device();
    return 0;
}

static void
module_a_exit(void)
{
    delete_device();
	return;
}

ssize_t 
module_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
    (void)filp;
    (void)buf;
    (void)f_pos;

    switch((enum command_read_counts)count)
    {
        case command_init_obj:
            wobj_init(&obj, finalize);
            obj_live = 1;
            callback_was_called = 0;
            weak_ref_live = 0;
        break;
        case command_ref:
            if(!obj_live)
            {
                printk(KERN_ERR "Dead object cannot be referenced.\n");
                return -1;
            }
            wobj_ref(&obj);
        break;
        case command_unref:
            if(!obj_live)
            {
                printk(KERN_ERR "Dead object cannot be dereferenced.\n");
                return -1;
            }
            wobj_unref(&obj);
        break;
        case command_unref_final:
            if(!obj_live)
            {
                printk(KERN_ERR "Dead object cannot be finally dereferenced.\n");
                return -1;
            }
            wobj_unref_final(&obj);
        break;
        default:
            printk(KERN_ERR "Incorrect command for read.\n");
            return -1;
    };
    return count;
}
                
ssize_t 
module_write(struct file *filp, const char __user *buf, size_t count, 
	loff_t *f_pos)
{
    (void)filp;
    (void)buf;
    (void)f_pos;

    switch((enum command_write_counts)count)
    {
        case command_weak_ref:
            if(!obj_live)
            {
                printk(KERN_ERR "Dead object cannot be referenced(weak).\n");
                return -1;
            }
            wobj_weak_ref_init(&weak_ref, &obj, weak_ref_callback);
            callback_was_called = 0;
            weak_ref_live = 1;
        break;
        case command_weak_ref_ref:
            wobj_weak_ref_get(&weak_ref);
        break;
        case command_weak_ref_clear:
            if(!weak_ref_live)
            {
                printk(KERN_ERR "Dead weak reference cannot be cleared.\n");
                return -1;
            }
            wobj_weak_ref_clear(&weak_ref);
            weak_ref_live = 0;
        break;
        default:
            printk(KERN_ERR "Incorrect command for write.\n");
            return -1;
    };
    return count;
}

module_init(module_a_init);
module_exit(module_a_exit);