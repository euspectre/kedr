#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
//#include <linux/types.h>	/* size_t */
//#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
//#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>	/* copy_*_user */

#include "icpt_target.h"

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("Dual BSD/GPL");

/* parameters */
int icpt_target_major = ICPT_TARGET_MAJOR;
int icpt_target_minor = 0;
unsigned long icpt_target_bsize = ICPT_TARGET_BUFFER_SIZE;

/* ================================================================ */
/* Main operations - declarations */

int icpt_target_open(struct inode *inode, struct file *filp);

int icpt_target_release(struct inode *inode, struct file *filp);

int icpt_target_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg);
/* ================================================================ */

struct icpt_target_dev *icpt_target_device; /* created in icpt_target_init_module() */

struct file_operations icpt_target_fops = {
	.owner =    THIS_MODULE,
	.llseek =   no_llseek,
	.ioctl =    icpt_target_ioctl,
	.open =     icpt_target_open,
	.release =  icpt_target_release,
};

/* ================================================================ */
/* Set up the char_dev structure for the device. */
static void icpt_target_setup_cdevice(struct icpt_target_dev *dev)
{
	int err;
	int devno = MKDEV(icpt_target_major, icpt_target_minor);
    
	cdev_init(&dev->cdevice, &icpt_target_fops);
	dev->cdevice.owner = THIS_MODULE;
	dev->cdevice.ops = &icpt_target_fops;
	
	err = cdev_add(&dev->cdevice, devno, 1);
	if (err)
	{
		printk(KERN_NOTICE 
			"[icpt_target] Error %d while trying to add icpt_target",
			err);
	}
	else
	{
		dev->dev_added = 1;
	}
	return;
}

/* ================================================================ */
static void
icpt_target_cleanup_module(void)
{
	dev_t devno = MKDEV(icpt_target_major, icpt_target_minor);
	
	printk(KERN_ALERT "[icpt_target] Cleaning up\n");
	
	/* Get rid of our char dev entries */
	if (icpt_target_device != NULL) {
		kfree(icpt_target_device->data);
		kfree(icpt_target_device->aux);
		if (icpt_target_device->dev_added)
		{
			cdev_del(&icpt_target_device->cdevice);
		}
		kfree(icpt_target_device);
	}

	/* [NB] cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
	return;
}

static int __init
icpt_target_init_module(void)
{
	int result;
	dev_t dev = 0;
	
	printk(KERN_ALERT "[icpt_target] Initializing\n");
	
	if (icpt_target_major > 0) {
		dev = MKDEV(icpt_target_major, 0);
		result = register_chrdev_region(dev, 1, "icpt_target");
	} else {
		result = alloc_chrdev_region(&dev, 0, 1, "icpt_target");
		icpt_target_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "[icpt_target] can't get major number %d\n", icpt_target_major);
		return result;
	}
	
	/* Allocate the array of devices */
	icpt_target_device = (struct icpt_target_dev*)kmalloc(
		sizeof(struct icpt_target_dev), 
		GFP_KERNEL);
	if (icpt_target_device == NULL) {
		printk(KERN_ALERT 
			"[icpt_target] Failed to allocate memory for 'struct icpt_target_dev'\n");
		result = -ENOMEM;
		goto fail;
	}
	
	memset(icpt_target_device, 0, sizeof(struct icpt_target_dev));
	
	/* Initialize the device. */
	icpt_target_device->buffer_size = icpt_target_bsize;
	icpt_target_device->dev_added = 0; // overkill, already zeroed anyway
	
	/* memory is to be allocated in open() and ioctl() */
	icpt_target_device->data = NULL; 
	
	init_MUTEX(&icpt_target_device->sem);
	icpt_target_setup_cdevice(icpt_target_device);
	
	return 0; /* success */

fail:
	icpt_target_cleanup_module();
	return result;
}

module_init(icpt_target_init_module);
module_exit(icpt_target_cleanup_module);
/* ================================================================ */

int 
icpt_target_open(struct inode *inode, struct file *filp)
{
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	struct icpt_target_dev *dev = NULL;
	
	printk(KERN_WARNING "[icpt_target] open()\n");
	
	if (mj != icpt_target_major || mn != 0)
	{
		printk(KERN_WARNING "[icpt_target] No device found with MJ=%d and MN=%d\n", 
			mj, mn);
		return -ENODEV; /* No such device */
	}
	
	/* store a pointer to struct icpt_target_dev here for other methods */
	dev = icpt_target_device;
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdevice)
	{
		printk(KERN_WARNING "[icpt_target] open: internal error\n");
		return -ENODEV; /* No such device */
	}
	
	if (down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	
	/* if opened the 1st time, allocate the buffer */
	if (dev->data == NULL)
	{
		dev->data = (unsigned char*)kmalloc(
			dev->buffer_size, 
			GFP_KERNEL);
		if (dev->data == NULL)
		{
			printk(KERN_WARNING "[icpt_target] open: out of memory\n");
			return -ENOMEM;
		}
		memset(dev->data, 0, dev->buffer_size);
	}
	
	up(&dev->sem);
	return 0;
}

int 
icpt_target_release(struct inode *inode, struct file *filp)
{
	printk(KERN_WARNING "[icpt_target] release()\n");
	return 0;
}

int 
icpt_target_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	struct icpt_target_dev *dev = (struct icpt_target_dev *)filp->private_data;
	
	printk(KERN_WARNING "[icpt_target] ioctl() for MJ=%d and MN=%d\n", 
		imajor(inode), iminor(inode));

	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != ICPT_TARGET_IOCTL_MAGIC) return -ENOTTY;
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
	case ICPT_TARGET_IOCTL_GO:
		if (dev->aux != NULL) 
		{
			kfree(dev->aux);
			dev->aux = NULL;
		}
		dev->aux = (unsigned char*)kmalloc(
			dev->buffer_size, 
			GFP_KERNEL);
		if (dev->aux == NULL)
		{
			printk(KERN_WARNING "[icpt_target] ioctl: out of memory\n");
			retval = -ENOMEM;
		}
		else
		{
			memset(dev->aux, 0, dev->buffer_size);
		}
		break;
	default:  
		retval = -ENOTTY;
		break;
	}
	
	/* End critical section */
	up(&dev->sem);
	return retval;
}
/* ================================================================ */
