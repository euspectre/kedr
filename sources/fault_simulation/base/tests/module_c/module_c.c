#include <linux/module.h>

#include <linux/kernel.h>	/* printk() */

#include <kedr/fault_simulation/fsim_base.h>
#include <../common.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

extern int a_external_value;
static void set_to_35(void* indicator_state)
{
    *((int*)indicator_state) = 35;
}

static int indicator_return_29(void* indicator_state, void* user_data)
{
    (void)indicator_state;
    (void)user_data;
    
    return 29;
}


static int __init
module_c_init(void)
{
    if(kedr_fsim_indicator_set(read_point_name, indicator_return_29,
        "", THIS_MODULE, &a_external_value, set_to_35))
    {
        printk(KERN_ERR "Cannot set indicator for 'read' point.\n");
        return -1;
    }
    return 0;
}

static void
module_c_exit(void)
{
	return;
}

module_init(module_c_init);
module_exit(module_c_exit);