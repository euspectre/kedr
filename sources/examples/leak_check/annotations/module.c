/* module.c - almost the same as cfake.c from "sample_target" example. The
 * difference is, device_create() and device_destroy() are annotated with 
 * the special calls for LeakCheck to track these operations.
 *
 * This example demonstrates how to use LeakCheck API in a module that is
 * not a plugin to KEDR by itself. One common use case is annotating custom
 * resource allocation/deallocation operations in a module you develop or,
 * at least, can rebuild.
 * 
 * See also cfake_construct_device() and cfake_destroy_device() as well as
 * annotate_resource_*() below. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 *
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>

#include <asm/uaccess.h>

/* LeakCheck API is declared here. */
#include <kedr/leak_check/leak_check.h>

#include "cfake.h"

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ================================================================ */

/* Call this function AFTER a resource has been successfully allocated. 
 * 'addr' - the address of the resource in memory, 'size' - its size. */
static noinline void
annotate_resource_alloc(const void *addr, size_t size)
{
	kedr_lc_handle_alloc(THIS_MODULE, addr, size, 
		__builtin_return_address(0));
}

/* Call this function BEFORE a resource has been freed. 
 * 'addr' - the address of the resource in memory. */
static noinline void
annotate_resource_free(const void *addr)
{
	kedr_lc_handle_free(THIS_MODULE, addr, 
		__builtin_return_address(0));
}
/* ================================================================ */

#define CFAKE_DEVICE_NAME "cfake"

static int cfake_ndevices = CFAKE_NDEVICES;
static unsigned long cfake_buffer_size = CFAKE_BUFFER_SIZE;
static unsigned long cfake_block_size = CFAKE_BLOCK_SIZE;

module_param(cfake_ndevices, int, S_IRUGO);
module_param(cfake_buffer_size, ulong, S_IRUGO);
module_param(cfake_block_size, ulong, S_IRUGO);
/* ================================================================ */

static unsigned int cfake_major = 0;
static struct cfake_dev *cfake_devices = NULL;
static struct class *cfake_class = NULL;
/* ================================================================ */

int 
cfake_open(struct inode *inode, struct file *filp)
{
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	struct cfake_dev *dev = NULL;
	
	if (mj != cfake_major || mn < 0 || mn >= cfake_ndevices)
	{
		printk(KERN_WARNING "[target] "
			"No device found with minor=%d and major=%d\n", 
			mj, mn);
		return -ENODEV; 
	}
	
	dev = &cfake_devices[mn];
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdev)
	{
		printk(KERN_WARNING "[target] open: internal error\n");
		return -ENODEV;
	}
	
	if (dev->data == NULL)
	{
		dev->data = (unsigned char*)kzalloc(dev->buffer_size, GFP_KERNEL);
		if (dev->data == NULL)
		{
			printk(KERN_WARNING "[target] open(): out of memory\n");
			return -ENOMEM;
		}
	}
	return 0;
}

int 
cfake_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t 
cfake_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	ssize_t retval = 0;
	
	if (mutex_lock_killable(&dev->cfake_mutex))
		return -EINTR;
	
	if (*f_pos >= dev->buffer_size)
		goto out;
	
	if (*f_pos + count > dev->buffer_size)
		count = dev->buffer_size - *f_pos;
	
	if (count > dev->block_size)
		count = dev->block_size;
	
	if (copy_to_user(buf, &(dev->data[*f_pos]), count) != 0)
	{
		retval = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	retval = count;
	
out:
	mutex_unlock(&dev->cfake_mutex);
	return retval;
}
				
ssize_t 
cfake_write(struct file *filp, const char __user *buf, size_t count, 
	loff_t *f_pos)
{
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	ssize_t retval = 0;
	
	if (mutex_lock_killable(&dev->cfake_mutex))
		return -EINTR;
	
	if (*f_pos >= dev->buffer_size) {
		retval = -EINVAL;
		goto out;
	}
	
	if (*f_pos + count > dev->buffer_size)
		count = dev->buffer_size - *f_pos;
	
	if (count > dev->block_size)
		count = dev->block_size;
	
	if (copy_from_user(&(dev->data[*f_pos]), buf, count) != 0)
	{
		retval = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	retval = count;
	
out:
	mutex_unlock(&dev->cfake_mutex);
	return retval;
}

loff_t 
cfake_llseek(struct file *filp, loff_t off, int whence)
{
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	loff_t newpos = 0;
	
	switch(whence) {
	  case 0: /* SEEK_SET */
		newpos = off;
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	  case 2: /* SEEK_END */
		newpos = dev->buffer_size + off;
		break;

	  default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0 || newpos > dev->buffer_size) 
		return -EINVAL;
	
	filp->f_pos = newpos;
	return newpos;
}

struct file_operations cfake_fops = {
	.owner =    THIS_MODULE,
	.read =     cfake_read,
	.write =    cfake_write,
	.open =     cfake_open,
	.release =  cfake_release,
	.llseek =   cfake_llseek,
};

/* ================================================================ */
static int
cfake_construct_device(struct cfake_dev *dev, int minor, 
	struct class *class)
{
	int err = 0;
	dev_t devno = MKDEV(cfake_major, minor);
	struct device *device;
	
	BUG_ON(dev == NULL || class == NULL);

	dev->data = NULL;     
	dev->buffer_size = cfake_buffer_size;
	dev->block_size = cfake_block_size;
	mutex_init(&dev->cfake_mutex);
	
	cdev_init(&dev->cdev, &cfake_fops);
	dev->cdev.owner = THIS_MODULE;
	
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
	{
		printk(KERN_WARNING "[target] Error %d while trying to add %s%d",
			err, CFAKE_DEVICE_NAME, minor);
		return err;
	}

	device = device_create(class, NULL, devno, NULL, 
		CFAKE_DEVICE_NAME "%d", minor);

	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		printk(KERN_WARNING "[target] Error %d while trying to create %s%d",
			err, CFAKE_DEVICE_NAME, minor);
		cdev_del(&dev->cdev);
		return err;
	}
	dev->device = device;
	
	/* The instance of struct device has been successfully created. 
	 * Let LeakCheck know about it. 
	 * Note that we use sizeof(struct device) as the size of the 
	 * newly allocated resource. It is not mandatory but highly
	 * recommended to provide a reasonable value for 'size'. */
	annotate_resource_alloc(dev->device, sizeof(struct device));
	
	return 0;
}

/* Destroy the device and free its buffer */
static void
cfake_destroy_device(struct cfake_dev *dev, int minor,
	struct class *class)
{
	BUG_ON(dev == NULL || class == NULL);
	
	/* The instance of struct device is about to be destroyed 
	 * (freed, released, whatever). Let LeakCheck know about it. */
	annotate_resource_free(dev->device);
	device_destroy(class, MKDEV(cfake_major, minor));
	
	cdev_del(&dev->cdev);
	kfree(dev->data);
	return;
}

/* ================================================================ */
static void
cfake_cleanup_module(int devices_to_destroy)
{
	int i;
	
	if (cfake_devices) {
		for (i = 0; i < devices_to_destroy; ++i) {
			cfake_destroy_device(&cfake_devices[i], i, cfake_class);
		}
		kfree(cfake_devices);
	}
	
	if (cfake_class)
		class_destroy(cfake_class);

	unregister_chrdev_region(MKDEV(cfake_major, 0), cfake_ndevices);
	return;
}

static int __init
cfake_init_module(void)
{
	int err = 0;
	int i = 0;
	int devices_to_destroy = 0;
	dev_t dev = 0;
	
	if (cfake_ndevices <= 0)
	{
		printk(KERN_WARNING "[target] Invalid value of cfake_ndevices: %d\n", 
			cfake_ndevices);
		err = -EINVAL;
		return err;
	}

	err = alloc_chrdev_region(&dev, 0, cfake_ndevices, CFAKE_DEVICE_NAME);
	if (err < 0) {
		printk(KERN_WARNING "[target] alloc_chrdev_region() failed\n");
		return err;
	}
	cfake_major = MAJOR(dev);

	cfake_class = class_create(THIS_MODULE, CFAKE_DEVICE_NAME);
	if (IS_ERR(cfake_class)) {
		err = PTR_ERR(cfake_class);
		goto fail;
	}

	cfake_devices = (struct cfake_dev *)kzalloc(
		cfake_ndevices * sizeof(struct cfake_dev), 
		GFP_KERNEL);
	if (cfake_devices == NULL) {
		err = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < cfake_ndevices; ++i) {
		err = cfake_construct_device(&cfake_devices[i], i, cfake_class);
		if (err) {
			devices_to_destroy = i;
			goto fail;
		}
	}
	return 0;

fail:
	cfake_cleanup_module(devices_to_destroy);
	return err;
}

static void __exit
cfake_exit_module(void)
{
	cfake_cleanup_module(cfake_ndevices);
	return;
}

module_init(cfake_init_module);
module_exit(cfake_exit_module);
/* ================================================================ */
