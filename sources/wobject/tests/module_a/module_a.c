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
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/delay.h>    /* ssleep() */

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <linux/errno.h>
#include <linux/err.h>
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
static int module_major = 0;
static int module_minor = 0;

struct cdev module_device;
static struct class *dev_class = NULL;

static ssize_t 
module_read(struct file *filp, char __user *buf, size_t count, 
    loff_t *f_pos);

static ssize_t
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
    struct device *d;
    
    if(alloc_chrdev_region(&dev, module_minor, 1, device_name) < 0)
    {
        printk(KERN_ERR "[module_a] Cannot allocate device number for test");
        return 1;
    }
    module_major = MAJOR(dev);
    
    cdev_init(&module_device, &module_fops);
    module_device.owner = THIS_MODULE;
    
    if(cdev_add(&module_device, dev, 1))
    {
        printk(KERN_ERR "[module_a] Cannot add device for test");
        goto fail1;
    }
    
    dev_class = class_create(THIS_MODULE, device_name);    
    if (IS_ERR(dev_class)) {
        printk(KERN_ERR "[module_a] "
            "Failed to create device class, error=%d\n",
            (int)(PTR_ERR(dev_class)));
        dev_class = NULL;
        goto fail2;
    }
    
    d = device_create(dev_class, NULL, dev, NULL, device_name);
    if (IS_ERR(d)) {
        printk(KERN_ERR "[module_a] "
            "Failed to create device node, error=%d\n",
            (int)(PTR_ERR(d)));
        goto fail3;
    }
    
    return 0;

fail3:
    class_destroy(dev_class);
fail2:
    cdev_del(&module_device);
fail1:
        unregister_chrdev_region(dev, 1);
        return 1;
}

static void delete_device(void)
{
    dev_t dev = MKDEV(module_major, module_minor);
    
    device_destroy(dev_class, dev);
    class_destroy(dev_class);
        
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