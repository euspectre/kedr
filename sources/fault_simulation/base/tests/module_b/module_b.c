#include <linux/module.h>

#include <linux/kernel.h>	/* printk() */

#include <kedr/fault_simulation/fsim_base.h>
#include <../common.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

extern int a_external_value;
static void set_to_5(void* indicator_state)
{
    *((int*)indicator_state) = 5;
}
static int indicator_return_9(void* indicator_state, void* user_data)
{
    (void)indicator_state;
    (void)user_data;
    
    return 9;
}

static int indicator_return_1arg(void* indicator_state, void* user_data)
{
    printk(KERN_INFO "Indicator was called.\n");
    (void)indicator_state;
    
    //return (int)*((size_t*)user_data);
    return 16;
}


static int __init
module_b_init(void)
{
    if(kedr_fsim_indicator_set(read_point_name, indicator_return_9,
        "", THIS_MODULE, &a_external_value, set_to_5))
    {
        printk(KERN_ERR "Cannot set indicator for 'read' point.\n");
        return 1;
    }
    if(kedr_fsim_indicator_set(write_point_name, indicator_return_1arg,
        "size_t", THIS_MODULE, NULL, NULL))
    {
        printk(KERN_ERR "Cannot set indicator for 'write' point.\n");
        return 1;
    }
    return 0;
}

static void
module_b_exit(void)
{
	return;
}

module_init(module_b_init);
module_exit(module_b_exit);