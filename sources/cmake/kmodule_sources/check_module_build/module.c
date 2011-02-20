#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

/* 
 * This bare skeleton of a kernel module is used to check if the system has
 * everything necessary to build at least such simple kernel modules.
 */
static int __init
my_init(void)
{
	printk(KERN_DEBUG "Initializing test module\n");
	return 0;
}

static void __exit
my_exit(void)
{
	printk(KERN_DEBUG "Cleaning up test module\n");
}

module_init(my_init);
module_exit(my_exit);
