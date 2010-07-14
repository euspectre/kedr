#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/cdev.h>

#include "trigger_common.h"

<$include : join(\n)$>

enum
{
	first_item = 0,
    <$enum_item : join(,\n\t)$>
};

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

// Major device number, minor always 0
int trigger_major = 0;
/* ================================================================ */
/* Main operations - declarations */

int 
trigger_ioctl(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg);

/* ================================================================ */

struct cdev trigger_device;

struct file_operations trigger_fops = {
	.owner =    THIS_MODULE,
	.ioctl =    trigger_ioctl,
};

/* ================================================================ */
static void
trigger_cleanup_module(void)
{
	dev_t devno = MKDEV(trigger_major, 0);
	
	printk(KERN_ALERT "[trigger_module] Cleaning up\n");
	
    cdev_del(&trigger_device);

	unregister_chrdev_region(devno, 1);
	return;
}

static int __init
trigger_init_module(void)
{
	dev_t dev = 0;
	if(alloc_chrdev_region(&dev, 0, 1, "kedr_trigger_device") < 0)
    {
	    printk(KERN_WARNING "[trigger_module] alloc_chrdev_region hasn't allocated device number.\n");
		return -1;
	}
    trigger_major = MAJOR(dev);

    cdev_init(&trigger_device, &trigger_fops);
    trigger_device.owner = THIS_MODULE;
    
    if(cdev_add(&trigger_device, dev, 1))
    {
	    printk(KERN_WARNING "[trigger_module] cdev_add hasn't added device.\n");
        unregister_chrdev_region(dev, 1);
		return -1;
    
    }
    	
	return 0; /* success */

}

module_init(trigger_init_module);
module_exit(trigger_cleanup_module);
/* ================================================================ */

int 
trigger_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
	void* __user user_area = (void* __user)arg;
	if (_IOC_TYPE(cmd) != TRIGGER_IOCTL_MAGIC) return -ENOTTY;
	if (_IOC_DIR(cmd) != (_IOC_READ | _IOC_WRITE)) return -ENOTTY;
	switch(_IOC_NR(cmd))
	{
<$case_block : join(\n)$>
	default:
		return -ENOTTY;
	}

	return 0;
}

/* ================================================================ */
