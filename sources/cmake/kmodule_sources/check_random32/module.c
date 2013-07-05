#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/random.h>

MODULE_LICENSE("GPL");

static int __init
my_init(void)
{
	return (int)random32();
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
