#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/delay.h>    /* ssleep() */

#include <linux/fs.h>		/* everything... */
#include <linux/cdev.h>

#include <linux/mutex.h> /* mutexes */

#include <kedr/module_weak_ref/module_weak_ref.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

/*
 * Use cases (with other module 'b' loaded):
 *
 * - unload_b(nothing to test)
 *
 * - register, unload_b(whether callback was called)
 *
 * - register, read, unload_b(whether weak_unref is work)
 *
 * - register, write, read&unload_b(whether module_weak_ref_wait is work)
 */

const char* device_name = "kedr_test_device";

int current_value = 0;
module_param(current_value, int, S_IRUGO);
//module which use exported functionality
struct module* m_user = NULL;

DECLARE_MUTEX(mutex_x);//for wait read after write was called
DECLARE_MUTEX(mutex_y);//protect current_value and m_user

int was_written = 0;//whether write was called, is not synchronized at all!

void a_unregister_module_callback(struct module* m, void* user_data)
{
    (void)m;

    if(was_written)
    {
        up(&mutex_x);
    }
    down(&mutex_y);

    ssleep(1);//as if do something long
    m_user = NULL;
    *((int*)user_data) -=5;
    
    up(&mutex_y);
}
void a_register_module(struct module* m)
{
    down(&mutex_y);

    BUG_ON(m_user != NULL);
    m_user = m;
    current_value+=5;

    module_weak_ref(m, a_unregister_module_callback, &current_value);

    up(&mutex_y);
}
EXPORT_SYMBOL(a_register_module);

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
module_a_init(void)
{
    module_weak_ref_init();
    init_device();
    return 0;
}

static void
module_a_exit(void)
{
    delete_device();
    module_weak_ref_destroy();
	return;
}

ssize_t 
module_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
    (void)filp;
    (void)buf;
    (void)f_pos;

    if(m_user != NULL)
    {
        down(&mutex_x);
        up(&mutex_x);

        if(was_written)
        {
            was_written = 0;
        }
        else
        {
            down(&mutex_y);
        }

        if(module_weak_unref(m_user, a_unregister_module_callback, &current_value))
        {
            up(&mutex_y);
            module_weak_ref_wait();
        }
        else
        {
            up(&mutex_y);
            current_value-=5;
        }
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

    if(was_written ==0)
    {
        was_written = 1;
        down(&mutex_x);
        down(&mutex_y);
    }

    return count;
}

module_init(module_a_init);
module_exit(module_a_exit);