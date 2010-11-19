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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/cdev.h>

#include <asm/uaccess.h>	/* copy_*_user */

#include "cfake.h"

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");

/* parameters */
int cfake_major = CFAKE_MAJOR;
int cfake_minor = 0;
int cfake_ndevices = CFAKE_NDEVICES;
unsigned long cfake_buffer_size = CFAKE_BUFFER_SIZE;
unsigned long cfake_block_size = CFAKE_BLOCK_SIZE;

module_param(cfake_major, int, S_IRUGO);
module_param(cfake_minor, int, S_IRUGO);
module_param(cfake_ndevices, int, S_IRUGO);
module_param(cfake_buffer_size, ulong, S_IRUGO);
module_param(cfake_block_size, ulong, S_IRUGO);

/* ================================================================ */
/* Main operations - declarations */

int 
cfake_open(struct inode *inode, struct file *filp);

int 
cfake_release(struct inode *inode, struct file *filp);

ssize_t 
cfake_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos);

ssize_t 
cfake_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos);

int 
cfake_ioctl(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg);

loff_t 
cfake_llseek(struct file *filp, loff_t off, int whence);

/* ================================================================ */

struct cfake_dev *cfake_devices;	/* created in cfake_init_module() */

struct file_operations cfake_fops = {
	.owner =    THIS_MODULE,
	.llseek =   cfake_llseek,
	.read =     cfake_read,
	.write =    cfake_write,
	.ioctl =    cfake_ioctl,
	.open =     cfake_open,
	.release =  cfake_release,
};

/* ================================================================ */
/* Set up the char_dev structure for the device. */
static void cfake_setup_cdevice(struct cfake_dev *dev, int index)
{
	int err;
	int devno = MKDEV(cfake_major, cfake_minor + index);
    
	cdev_init(&dev->cdevice, &cfake_fops);
	dev->cdevice.owner = THIS_MODULE;
	dev->cdevice.ops = &cfake_fops;
	
	err = cdev_add(&dev->cdevice, devno, 1);
	if (err)
	{
		printk(KERN_NOTICE "[cr_target] Error %d while trying to add cfake%d",
			err, index);
	}
	else
	{
		dev->dev_added = 1;
	}
	return;
}

/* ================================================================ */
static void
cfake_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(cfake_major, cfake_minor);
	
	/* Get rid of our char dev entries */
	if (cfake_devices) {
		for (i = 0; i < cfake_ndevices; ++i) {
			kfree(cfake_devices[i].data);
			if (cfake_devices[i].dev_added)
			{
				cdev_del(&cfake_devices[i].cdevice);
			}
		}
		kfree(cfake_devices);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, cfake_ndevices);
	return;
}

static int __init
cfake_init_module(void)
{
	int result = 0;
	int i;
	dev_t dev = 0;
	
	if (cfake_ndevices <= 0)
	{
		printk(KERN_WARNING "[cr_target] Invalid value of cfake_ndevices: %d\n", 
			cfake_ndevices);
		result = -EINVAL;
		return result;
	}
	
	/* Get a range of minor numbers to work with, asking for a dynamic
	major number unless directed otherwise at load time.
	*/
	if (cfake_major > 0) {
		dev = MKDEV(cfake_major, cfake_minor);
		result = register_chrdev_region(dev, cfake_ndevices, "cfake");
	} else {
		result = alloc_chrdev_region(&dev, cfake_minor, cfake_ndevices,
				"cfake");
		cfake_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "[cr_target] can't get major number %d\n", cfake_major);
		return result;
	}
	
	/* Allocate the array of devices */
	cfake_devices = (struct cfake_dev*)kmalloc(
		cfake_ndevices * sizeof(struct cfake_dev), 
		GFP_KERNEL);
	if (cfake_devices == NULL) {
		result = -ENOMEM;
		goto fail;
	}
	memset(cfake_devices, 0, cfake_ndevices * sizeof(struct cfake_dev));
	
	/* Initialize each device. */
	for (i = 0; i < cfake_ndevices; ++i) {
		cfake_devices[i].buffer_size = cfake_buffer_size;
		cfake_devices[i].block_size = cfake_block_size;
		cfake_devices[i].dev_added = 0;
		
		/* memory is to be allocated in open() */
		cfake_devices[i].data = NULL; 
		
		init_MUTEX(&cfake_devices[i].sem);
		cfake_setup_cdevice(&cfake_devices[i], i);
	}
	
	return 0; /* success */

fail:
	cfake_cleanup_module();
	return result;
}

static void __exit
cfake_exit_module(void)
{
	cfake_cleanup_module();
	return;
}

module_init(cfake_init_module);
module_exit(cfake_exit_module);
/* ================================================================ */

int 
cfake_open(struct inode *inode, struct file *filp)
{
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	struct cfake_dev *dev = NULL;
	
	if (mj != cfake_major || mn < cfake_minor || 
		mn >= cfake_minor + cfake_ndevices)
	{
		printk(KERN_WARNING "[cr_target] No device found with MJ=%d and MN=%d\n", 
			mj, mn);
		return -ENODEV; /* No such device */
	}
	
	/* store a pointer to struct cfake_dev here for other methods */
	dev = &cfake_devices[mn - cfake_minor];
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdevice)
	{
		printk(KERN_WARNING "[cr_target] open: internal error\n");
		return -ENODEV; /* No such device */
	}
	
	/* if opened the 1st time, allocate the buffer */
	if (dev->data == NULL)
	{
		dev->data = (unsigned char*)kmalloc(
			dev->buffer_size, 
			GFP_KERNEL);
		if (dev->data == NULL)
		{
			printk(KERN_WARNING "[cr_target] open: out of memory\n");
			return -ENOMEM;
		}
		memset(dev->data, 0, dev->buffer_size);
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
	
	if (down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	
	if (*f_pos >= dev->buffer_size) /* EOF */
	{
		goto out;
	}
	
	if (*f_pos + count > dev->buffer_size)
	{
		count = dev->buffer_size - *f_pos;
	}
	
	if (count > dev->block_size)
	{
		count = dev->block_size;
	}
	
	if (copy_to_user(buf, &(dev->data[*f_pos]), count) != 0)
	{
		retval = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	retval = count;
	
out:
  	up(&dev->sem);
	return retval;
}
                
ssize_t 
cfake_write(struct file *filp, const char __user *buf, size_t count, 
	loff_t *f_pos)
{
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	ssize_t retval = 0;
	
	if (down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	
	if (*f_pos >= dev->buffer_size) /* EOF */
	{
		goto out;
	}
	
	if (*f_pos + count > dev->buffer_size)
	{
		count = dev->buffer_size - *f_pos;
	}
	
	if (count > dev->block_size)
	{
		count = dev->block_size;
	}
	
	if (copy_from_user(&(dev->data[*f_pos]), buf, count) != 0)
	{
		retval = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	retval = count;
	
out:
  	up(&dev->sem);
	return retval;
}

int 
cfake_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	int fill_char;
	unsigned int block_size = 0;
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != CFAKE_IOCTL_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > CFAKE_IOCTL_NCODES) return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}
	if (err) 
	{
		return -EFAULT;
	}
	
	/* Begin critical section */
	if (down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	
	switch(cmd) {
	case CFAKE_IOCTL_RESET:
		memset(dev->data, 0, dev->buffer_size);
		break;
	
	case CFAKE_IOCTL_FILL:
		retval = get_user(fill_char, (int __user *)arg);
		if (retval == 0) /* success */
		{
			memset(dev->data, fill_char, dev->buffer_size);
		}
		break;
	
	case CFAKE_IOCTL_LFIRM:
		/* Assume that only an administrator can load the 'firmware' */ 
		if (!capable(CAP_SYS_ADMIN))
		{
			retval = -EPERM;
			break;
		}
		
		memset(dev->data, 0, dev->buffer_size);
		strcpy((char*)(dev->data), "Hello, hacker!\n");
		break;
	
	case CFAKE_IOCTL_RBUFSIZE:
		retval = put_user(dev->buffer_size, (unsigned long __user *)arg);
		break;
	
	case CFAKE_IOCTL_SBLKSIZE:
		retval = get_user(block_size, (unsigned long __user *)arg);
		if (retval != 0) break;
		
		retval = put_user(dev->block_size, (unsigned long __user *)arg);
		if (retval != 0) break;
		
		dev->block_size = block_size;
		break;
	
	default:  /* redundant, as 'cmd' was checked against CFAKE_IOCTL_NCODES */
		retval = -ENOTTY;
	}
	
	/* End critical section */
	up(&dev->sem);
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
	if (newpos < 0) return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}

/* ================================================================ */
