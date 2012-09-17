/* cfake.c - implementation of a simple module for a character device 
 * can be used for testing, demonstrations, etc.
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

#include "cfake.h"

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");

#define CFAKE_DEVICE_NAME "cfake"

/* parameters */
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
		return -ENODEV; /* No such device */
	}
	
	/* store a pointer to struct cfake_dev here for other methods */
	dev = &cfake_devices[mn];
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdev)
	{
		printk(KERN_WARNING "[target] open: internal error\n");
		return -ENODEV; /* No such device */
	}
	
	/* if opened the 1st time, allocate the buffer */
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
	
	if (*f_pos >= dev->buffer_size) /* EOF */
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
	/* Writing beyond the end of the buffer is not allowed. */
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
/* Setup and register the device with specific index (the index is also
 * the minor number of the device).
 * Device class should be created beforehand.
 */
static int
cfake_construct_device(struct cfake_dev *dev, int minor, 
	struct class *class)
{
	int err = 0;
	dev_t devno = MKDEV(cfake_major, minor);
	struct device *device = NULL;
	
	BUG_ON(dev == NULL || class == NULL);

	/* Memory is to be allocated when the device is opened the first time */
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

	device = device_create(class, NULL, /* no parent device */ 
		devno, NULL, /* no additional data */
		CFAKE_DEVICE_NAME "%d", minor);

	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		printk(KERN_WARNING "[target] Error %d while trying to create %s%d",
			err, CFAKE_DEVICE_NAME, minor);
		cdev_del(&dev->cdev);
		return err;
	}
	return 0;
}

/* Destroy the device and free its buffer */
static void
cfake_destroy_device(struct cfake_dev *dev, int minor,
	struct class *class)
{
	BUG_ON(dev == NULL || class == NULL);
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
	
	/* Get rid of character devices (if any exist) */
	if (cfake_devices) {
		for (i = 0; i < devices_to_destroy; ++i) {
			cfake_destroy_device(&cfake_devices[i], i, cfake_class);
		}
		kfree(cfake_devices);
	}
	
	if (cfake_class)
		class_destroy(cfake_class);

	/* [NB] cfake_cleanup_module is never called if alloc_chrdev_region()
	 * has failed. */
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
	
	/* Get a range of minor numbers (starting with 0) to work with */
	err = alloc_chrdev_region(&dev, 0, cfake_ndevices, CFAKE_DEVICE_NAME);
	if (err < 0) {
		printk(KERN_WARNING "[target] alloc_chrdev_region() failed\n");
		return err;
	}
	cfake_major = MAJOR(dev);

	/* Create device class (before allocation of the array of devices) */
	cfake_class = class_create(THIS_MODULE, CFAKE_DEVICE_NAME);
	if (IS_ERR(cfake_class)) {
		err = PTR_ERR(cfake_class);
		goto fail;
	}
	
	/* Allocate the array of devices */
	cfake_devices = (struct cfake_dev *)kzalloc(
		cfake_ndevices * sizeof(struct cfake_dev), 
		GFP_KERNEL);
	if (cfake_devices == NULL) {
		err = -ENOMEM;
		goto fail;
	}
	
	/* Construct devices */
	for (i = 0; i < cfake_ndevices; ++i) {
		err = cfake_construct_device(&cfake_devices[i], i, cfake_class);
		if (err) {
			devices_to_destroy = i;
			goto fail;
		}
	}
	return 0; /* success */

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
