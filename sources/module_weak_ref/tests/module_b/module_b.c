#include <linux/module.h>

#include <linux/kernel.h>	/* printk() */

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

extern void a_register_module(struct module *m);

static int __init
module_b_init(void)
{
    a_register_module(THIS_MODULE);
    return 0;
}

static void
module_b_exit(void)
{
	return;
}

module_init(module_b_init);
module_exit(module_b_exit);