#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */

#include <linux/fs.h>		/* everything... */
#include <linux/cdev.h>

//#include <kedr/fault_simulation/fsim_base.h>
#include <kedr/fault_simulation/fault_simulation.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static const char* read_point_name = "kedr-read-point";
static const char* write_point_name = "kedr-write-point";

const char* device_name = "kedr_test_device";
//contain last value, returned by read_point or write_point simulation
int current_value = 0;
module_param(current_value, int, S_IRUGO);
//for set by other modules(e.g., when they unloaded)
//int a_external_value = 0;
//module_param(a_external_value, int, S_IRUGO);
//EXPORT_SYMBOL(a_external_value);

static struct kedr_simulation_point* read_point = NULL;
static struct kedr_simulation_point* write_point = NULL;
//Device
int module_major = 0;
int module_minor = 0;

struct cdev module_device;


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
    read_point = kedr_fsim_point_register(read_point_name, NULL, THIS_MODULE);
    if(read_point == NULL)
    {
        printk(KERN_ERR "Cannot register simulation point for read() for test.\n");
        return -1;
    }
    write_point = kedr_fsim_point_register(write_point_name, "size_t", THIS_MODULE);
    if(write_point == NULL)
    {
        printk(KERN_ERR "Cannot register simulation point for write() for test.\n");
        kedr_fsim_point_unregister(read_point);
        return -1;
    }
    if(init_device())
    {
        printk(KERN_ERR "Cannot create character device for test.\n");
        kedr_fsim_point_unregister(read_point);
        kedr_fsim_point_unregister(write_point);
        return -1;
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

    return count;
}

module_init(module_a_init);
module_exit(module_a_exit);