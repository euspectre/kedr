#include <linux/kernel.h>	/* printk() */
#include <linux/module.h>

#include <fault_simulation/fault_simulation.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

int fault_always(void* indicator_state, void* user_data)
{
	printk(KERN_INFO "Indicator was called.\n");
	return 1;
}

static void
fault_control_cleanup_module(void)
{

	printk(KERN_INFO "[fault_control] Stops\n");
	return;
}

static int __init
fault_control_init_module(void)
{
	printk(KERN_INFO "[fault_control] Starts\n");
	return kedr_fsim_set_indicator("kmalloc", fault_always, "",
        THIS_MODULE, NULL, NULL);
}

module_init(fault_control_init_module);
module_exit(fault_control_cleanup_module);