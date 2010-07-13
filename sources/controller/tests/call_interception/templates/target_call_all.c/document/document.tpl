/*********************************************************************
 * Module: target_call_all
 *********************************************************************/
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");
/*********************************************************************/

<$include : join(\n)$>

//for copy_from_user/copy_to_user
char arg[10] = {1,2,3,4,5,6,7,8,9,0};

static void
target_cleanup_module(void)
{
	printk(KERN_INFO "[target_call_all] Cleanup complete\n");
	return;
}

static int __init
target_init_module(void)
{
	printk(KERN_INFO "[target_call_all] Initializing\n");
{
	<$code: join(\n}\n{\n)$>
}
	return 0;
}

module_init(target_init_module);
module_exit(target_cleanup_module);
/*********************************************************************/
