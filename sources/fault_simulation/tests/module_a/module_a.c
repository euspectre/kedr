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

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/err.h>

#include <kedr/fault_simulation/fault_simulation.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static const char* read_point_name = "kedr-read-point";
static const char* write_point_name = "kedr-write-point";

const char* device_name = "kedr_test_device";
//contain last value, returned by read_point or write_point simulation
int current_value = 0;
module_param(current_value, int, S_IRUGO);


static struct kedr_simulation_point* read_point = NULL;
static struct kedr_simulation_point* write_point = NULL;
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
    read_point = kedr_fsim_point_register(read_point_name, NULL);
    if(read_point == NULL)
    {
        printk(KERN_ERR "[module_a] "
            "Cannot register simulation point for read() for test.\n");
        return -EINVAL;
    }
    write_point = kedr_fsim_point_register(write_point_name, "size_t");
    if(write_point == NULL)
    {
        printk(KERN_ERR "[module_a] "
            "Cannot register simulation point for write() for test.\n");
        kedr_fsim_point_unregister(read_point);
        return -EINVAL;
    }
    if(init_device())
    {
        printk(KERN_ERR "[module_a] "
            "Cannot create character device for test.\n");
        kedr_fsim_point_unregister(read_point);
        kedr_fsim_point_unregister(write_point);
        return -EINVAL;
    }
    return 0;
}

static void
module_a_exit(void)
{
    delete_device();
    kedr_fsim_point_unregister(read_point);
    kedr_fsim_point_unregister(write_point);
	return;
}

ssize_t 
module_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
    (void)filp;
    (void)buf;
    (void)f_pos;

    current_value = kedr_fsim_point_simulate(read_point, NULL);
    if(current_value)
        kedr_fsim_fault_message("Read: %d", current_value);
    return count;
}
                
ssize_t 
module_write(struct file *filp, const char __user *buf, size_t count, 
	loff_t *f_pos)
{
    (void)filp;
    (void)buf;
    (void)f_pos;

    current_value = kedr_fsim_point_simulate(write_point, &count);
    if(current_value)
        kedr_fsim_fault_message("Write for %d: %d", count, current_value);
    
    return count;
}

module_init(module_a_init);
module_exit(module_a_exit);