#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <linux/uaccess.h>

#include <linux/fs.h> /*file operations*/
#include <linux/cdev.h> /*character device definition*/
#include <linux/device.h> /*class_create*/
#include <linux/string.h>
#include <linux/err.h>
#include <linux/slab.h>

struct tt_dev
{
	struct cdev cdev;
	char data[10];
	struct mutex mutex;
};

int 
ttd_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

ssize_t 
ttd_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	struct tt_dev* sdev = container_of(filp->f_dentry->d_inode->i_cdev,
		struct tt_dev, cdev);

	if (*f_pos < 0)
		return -EINVAL;
	if (*f_pos >= sizeof(sdev->data) || !count)
		return 0;
	if (count > sizeof(sdev->data) - *f_pos)
		count = sizeof(sdev->data) - *f_pos;

	mutex_lock(&sdev->mutex);
	if(copy_to_user(buf, sdev->data + *f_pos, count))
	{
		mutex_unlock(&sdev->mutex);
		return -EFAULT;
	}
	mutex_unlock(&sdev->mutex);

	*f_pos += count;
	return count;
}

ssize_t 
ttd_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
    void *raw = NULL;
    char *str = NULL;
    
	struct tt_dev* sdev = container_of(filp->f_dentry->d_inode->i_cdev,
		struct tt_dev, cdev);

	if (*f_pos < 0)
		return -EINVAL;
	if (*f_pos >= sizeof(sdev->data) || !count)
		return -EINVAL;
	if (count > sizeof(sdev->data) - *f_pos)
		count = sizeof(sdev->data) - *f_pos;
	mutex_lock(&sdev->mutex);
	if(copy_from_user(sdev->data + *f_pos, buf, count))
	{
		mutex_unlock(&sdev->mutex);
		return -EFAULT;
	}
    
    /* trigger memdup_user() and strndup_user */
    raw = memdup_user(buf, 1);
    if (!IS_ERR(raw)) {
    	kfree(raw);
    } else {
    	printk(KERN_INFO 
    "Failed to copy data from user space with memdup_user(), errno=%d.", 
    		(int)PTR_ERR(raw)); 
    }
    
    str = strndup_user(buf, 1);
    if (!IS_ERR(str)) {
    	kfree(str);
    } else {
    	printk(KERN_INFO 
    "Failed to copy data from user space with strndup_user(), errno=%d.", 
    		(int)PTR_ERR(str)); 
    }
    
	mutex_unlock(&sdev->mutex);
	*f_pos += count;
	
	return count;
}

struct file_operations ttd_ops =
{
	.owner = THIS_MODULE,
	.open = ttd_open,
	.read = ttd_read,
	.write = ttd_write
};

struct tt_dev dev;
struct class* ttd_class;
struct device* ttd_dev;

int ttd_minor = 0;
int ttd_major;

int __init
trigger_target_init(void)
{
	int result;
	dev_t devno = 0;
	
	memset(dev.data, 0, sizeof(dev.data));
	mutex_init(&dev.mutex);
	
	result = alloc_chrdev_region(&devno, ttd_minor, 1, "ttd");
	if(result)
	{
		pr_err("Cannot register character device region.");
		return result;
	}
	ttd_major = MAJOR(devno);
	
	cdev_init(&dev.cdev, &ttd_ops);
	dev.cdev.owner = THIS_MODULE;
	dev.cdev.ops = &ttd_ops;
	
	ttd_class = class_create(THIS_MODULE, "ttd");
	if(IS_ERR(ttd_class))
	{
		pr_err("Cannot create class.");
		unregister_chrdev_region(devno, 1);
		return PTR_ERR(ttd_class);
	}
	
	result = cdev_add(&dev.cdev, devno, 1);
	if(result)
	{
		pr_err("Cannot add character device.");
		class_destroy(ttd_class);
		unregister_chrdev_region(devno, 1);
		return result;
	}
	ttd_dev = device_create(ttd_class, NULL, devno, NULL, "ttd");
	if(IS_ERR(ttd_dev))
	{
		pr_err("Cannot create file for added character device");
		cdev_del(&dev.cdev);
		class_destroy(ttd_class);
		unregister_chrdev_region(devno, 1);
		return PTR_ERR(ttd_dev);
	}

	return 0;
}
void
trigger_target_exit(void)
{
	dev_t devno = MKDEV(ttd_major, ttd_minor);;
	device_destroy(ttd_class, devno);
	cdev_del(&dev.cdev);
	class_destroy(ttd_class);
	unregister_chrdev_region(devno, 1);
}

module_init(trigger_target_init);
module_exit(trigger_target_exit);