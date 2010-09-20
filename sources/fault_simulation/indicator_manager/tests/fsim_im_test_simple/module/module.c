#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h> /* kmalloc() */

#include <kedr/fault_simulation/fsim_base.h>
#include <kedr/fault_simulation/fsim_indicator_manager.h>

#include <../common.h>

#include <linux/fs.h>		/* everything... */
#include <linux/cdev.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static char str_result[100] = "";
module_param_string(result, str_result, sizeof(str_result), S_IRUGO);

#define SET_RESULT(str, ...) snprintf(str_result, sizeof(str_result), str, __VA_ARGS__)
#define SET_RESULT0(str) SET_RESULT("%s", str)

struct kedr_simulation_point* point = NULL;

static int indicator_return_2(void* indicator_state, void* user_data)
{
    (void)indicator_state;
    (void)user_data;
    return 2;
}

//Device
int module_major = 0;
int module_minor = 0;

struct cdev module_device;


ssize_t 
module_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos);

ssize_t
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
module_this_init(void)
{
    if(init_device()) return 1;
    
    point = kedr_fsim_point_register(point_name, NULL);
    if(point == NULL)
    {
        SET_RESULT0("Cannot register simulation point");
        delete_device();
        return 0;
    }
    if(kedr_fsim_indicator_function_register(indicator_name,
        indicator_return_2, NULL, THIS_MODULE,
        NULL, NULL))
    {
        SET_RESULT0("Cannot register indicator function.");
        kedr_fsim_point_unregister(point);
        delete_device();
        return 0;
    }
    SET_RESULT0("Ok");
    return 0;
}

static void
module_this_exit(void)
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

    
    //some expression for test
    if(kedr_fsim_simulate(point, NULL) == count)
    {
        SET_RESULT0("Ok");
    }
    else
    {
        SET_RESULT0("Incorrect indicator was called or no indicator function was called at all.");
    }
    return count;
}
                
ssize_t 
module_write(struct file *filp, const char __user *buf, size_t count, 
	loff_t *f_pos)
{
    (void)filp;
    (void)buf;
    (void)f_pos;

    return count;
}

module_init(module_this_init);
module_exit(module_this_exit);
