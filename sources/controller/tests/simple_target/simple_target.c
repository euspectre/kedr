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

#include "simple_target.h"

MODULE_AUTHOR("Eugene");

/* Because this module uses the instruction decoder which is distributed
 * under GPL, I have no choice but to distribute this module under GPL too.
 * */
MODULE_LICENSE("GPL");

/* parameters */
int kedr_test_major = KEDR_TEST_MAJOR;
int kedr_test_minor = 0;
int kedr_test_ndevices = KEDR_TEST_NDEVICES;
unsigned long kedr_test_buffer_size = KEDR_TEST_BUFFER_SIZE;
unsigned long kedr_test_block_size = KEDR_TEST_BLOCK_SIZE;

module_param(kedr_test_major, int, S_IRUGO);
module_param(kedr_test_minor, int, S_IRUGO);
module_param(kedr_test_ndevices, int, S_IRUGO);
module_param(kedr_test_buffer_size, ulong, S_IRUGO);
module_param(kedr_test_block_size, ulong, S_IRUGO);

/* ================================================================ */
/* Main operations - declarations */

int 
kedr_test_open(struct inode *inode, struct file *filp);

int 
kedr_test_release(struct inode *inode, struct file *filp);

ssize_t 
kedr_test_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos);

ssize_t 
kedr_test_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos);

int 
kedr_test_ioctl(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg);

loff_t 
kedr_test_llseek(struct file *filp, loff_t off, int whence);

/* ================================================================ */

struct kedr_test_dev *kedr_test_devices;	/* created in kedr_test_init_module() */

struct file_operations kedr_test_fops = {
	.owner =    THIS_MODULE,
	.llseek =   kedr_test_llseek,
	.read =     kedr_test_read,
	.write =    kedr_test_write,
	.ioctl =    kedr_test_ioctl,
	.open =     kedr_test_open,
	.release =  kedr_test_release,
};

/* ================================================================ */
/* Set up the char_dev structure for the device. */
static void kedr_test_setup_cdevice(struct kedr_test_dev *dev, int index)
{
	int err;
	int devno = MKDEV(kedr_test_major, kedr_test_minor + index);
    
	cdev_init(&dev->cdevice, &kedr_test_fops);
	dev->cdevice.owner = THIS_MODULE;
	dev->cdevice.ops = &kedr_test_fops;
	
	err = cdev_add(&dev->cdevice, devno, 1);
	if (err)
	{
		printk(KERN_NOTICE "[simple_target] Error %d while trying to add kedr_test%d",
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
kedr_test_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(kedr_test_major, kedr_test_minor);
	
	printk(KERN_ALERT "[simple_target] Cleaning up\n");
	
	/* Get rid of our char dev entries */
	if (kedr_test_devices) {
		for (i = 0; i < kedr_test_ndevices; ++i) {
			kfree(kedr_test_devices[i].data);
			if (kedr_test_devices[i].dev_added)
			{
				cdev_del(&kedr_test_devices[i].cdevice);
			}
		}
		kfree(kedr_test_devices);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, kedr_test_ndevices);
	return;
}

static int __init
kedr_test_init_module(void)
{
	int result = 0;
	int i;
	dev_t dev = 0;
	
	printk(KERN_ALERT "[simple_target] Initializing\n");
	
	if (kedr_test_ndevices <= 0)
	{
		printk(KERN_WARNING "[simple_target] Invalid value of kedr_test_ndevices: %d\n", 
			kedr_test_ndevices);
		result = -EINVAL;
		return result;
	}
	
	/* Get a range of minor numbers to work with, asking for a dynamic
	major number unless directed otherwise at load time.
	*/
	if (kedr_test_major > 0) {
		dev = MKDEV(kedr_test_major, kedr_test_minor);
		result = register_chrdev_region(dev, kedr_test_ndevices, "kedr_test");
	} else {
		result = alloc_chrdev_region(&dev, kedr_test_minor, kedr_test_ndevices,
				"kedr_test");
		kedr_test_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "[simple_target] can't get major number %d\n", kedr_test_major);
		return result;
	}
	
	/* Allocate the array of devices */
	kedr_test_devices = (struct kedr_test_dev*)kmalloc(
		kedr_test_ndevices * sizeof(struct kedr_test_dev), 
		GFP_KERNEL);
	if (kedr_test_devices == NULL) {
		result = -ENOMEM;
		goto fail;
	}
	memset(kedr_test_devices, 0, kedr_test_ndevices * sizeof(struct kedr_test_dev));
	
	/* Initialize each device. */
	for (i = 0; i < kedr_test_ndevices; ++i) {
		kedr_test_devices[i].buffer_size = kedr_test_buffer_size;
		kedr_test_devices[i].block_size = kedr_test_block_size;
		kedr_test_devices[i].dev_added = 0;
		
		/* memory is to be allocated in open() */
		kedr_test_devices[i].data = NULL; 
		
		init_MUTEX(&kedr_test_devices[i].sem);
		kedr_test_setup_cdevice(&kedr_test_devices[i], i);
	}
	
	return 0; /* success */

fail:
	kedr_test_cleanup_module();
	return result;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/* ================================================================ */

int 
kedr_test_open(struct inode *inode, struct file *filp)
{
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	struct kedr_test_dev *dev = NULL;
	
	printk(KERN_WARNING "[simple_target] open() for MJ=%d and MN=%d\n", mj, mn);
	
	if (mj != kedr_test_major || mn < kedr_test_minor || 
		mn >= kedr_test_minor + kedr_test_ndevices)
	{
		printk(KERN_WARNING "[simple_target] No device found with MJ=%d and MN=%d\n", 
			mj, mn);
		return -ENODEV; /* No such device */
	}
	
	/* store a pointer to struct kedr_test_dev here for other methods */
	dev = &kedr_test_devices[mn - kedr_test_minor];
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdevice)
	{
		printk(KERN_WARNING "[simple_target] open: internal error\n");
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
			printk(KERN_WARNING "[simple_target] open: out of memory\n");
			return -ENOMEM;
		}
		memset(dev->data, 0, dev->buffer_size);
	}
	return 0;
}

int 
kedr_test_release(struct inode *inode, struct file *filp)
{
	printk(KERN_WARNING "[simple_target] release() for MJ=%d and MN=%d\n", 
		imajor(inode), iminor(inode));
	return 0;
}

ssize_t 
kedr_test_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
	struct kedr_test_dev *dev = (struct kedr_test_dev *)filp->private_data;
	ssize_t retval = 0;
	
	printk(KERN_WARNING "[simple_target] read() for MJ=%d and MN=%d\n", 
		imajor(filp->f_dentry->d_inode), iminor(filp->f_dentry->d_inode));

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
kedr_test_write(struct file *filp, const char __user *buf, size_t count, 
	loff_t *f_pos)
{
	struct kedr_test_dev *dev = (struct kedr_test_dev *)filp->private_data;
	ssize_t retval = 0;
	
	printk(KERN_WARNING "[simple_target] write() for MJ=%d and MN=%d\n", 
		imajor(filp->f_dentry->d_inode), iminor(filp->f_dentry->d_inode));
	
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
kedr_test_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	int fill_char;
	unsigned int block_size = 0;
	struct kedr_test_dev *dev = (struct kedr_test_dev *)filp->private_data;
	
	printk(KERN_WARNING "[simple_target] ioctl() for MJ=%d and MN=%d\n", 
		imajor(inode), iminor(inode));

	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != KEDR_TEST_IOCTL_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > KEDR_TEST_IOCTL_NCODES) return -ENOTTY;

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
	case KEDR_TEST_IOCTL_RESET:
		memset(dev->data, 0, dev->buffer_size);
		break;
	
	case KEDR_TEST_IOCTL_FILL:
		retval = get_user(fill_char, (int __user *)arg);
		if (retval == 0) /* success */
		{
			memset(dev->data, fill_char, dev->buffer_size);
		}
		break;
	
	case KEDR_TEST_IOCTL_LFIRM:
		/* Assume that only an administrator can load the 'firmware' */ 
		if (!capable(CAP_SYS_ADMIN))
		{
			retval = -EPERM;
			break;
		}
		
		memset(dev->data, 0, dev->buffer_size);
		strcpy((char*)(dev->data), "Hello, hacker!\n");
		break;
	
	case KEDR_TEST_IOCTL_RBUFSIZE:
		retval = put_user(dev->buffer_size, (unsigned long __user *)arg);
		break;
	
	case KEDR_TEST_IOCTL_SBLKSIZE:
		retval = get_user(block_size, (unsigned long __user *)arg);
		if (retval != 0) break;
		
		retval = put_user(dev->block_size, (unsigned long __user *)arg);
		if (retval != 0) break;
		
		dev->block_size = block_size;
		break;
	
	default:  /* redundant, as 'cmd' was checked against KEDR_TEST_IOCTL_NCODES */
		retval = -ENOTTY;
	}
	
	/* End critical section */
	up(&dev->sem);
	return retval;
}

loff_t 
kedr_test_llseek(struct file *filp, loff_t off, int whence)
{
	struct kedr_test_dev *dev = (struct kedr_test_dev *)filp->private_data;
	loff_t newpos = 0;
	
	printk(KERN_WARNING "[simple_target] read() for MJ=%d and MN=%d\n", 
		imajor(filp->f_dentry->d_inode), iminor(filp->f_dentry->d_inode));
	
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
