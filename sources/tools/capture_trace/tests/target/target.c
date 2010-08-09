/*********************************************************************
 * Module: kedr_target
 *********************************************************************/
#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>	    /* kmalloc(), kfree() */

/* ================================================================ */
static void
kedr_target_cleanup_module(void)
{
    void* p = kmalloc(100, GFP_KERNEL);
    kfree(p);
	return;
}

static int __init
kedr_target_init_module(void)
{
    void* p = kmalloc(100, GFP_KERNEL);
    kfree(p);
    	
	return 0; /* success */

}

module_init(kedr_target_init_module);
module_exit(kedr_target_cleanup_module);
