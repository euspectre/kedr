#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/*********************************************************************/

#include <kedr/controller/controller_common.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/errno.h>    /* error codes */

/*********************************************************************
 * Replacement functions
 *********************************************************************/
static void*
repl___kmalloc(size_t size, gfp_t flags)
{
	void* returnValue;

	/* Call the target function */
	returnValue = __kmalloc(size, flags);
    /* Do nothing more */

	return returnValue;
}

static void
repl_kfree(void* p)
{
    /* Call the target function */
	kfree(p);
	return;
}
/*********************************************************************/

/* Names and addresses of the functions of interest */
static void* target_func_addrs[] = {
	(void*)&__kmalloc,
	(void*)&kfree
};

/* Addresses of the replacement functions */
static void* repl_func_addrs[] = {
	(void*)&repl___kmalloc,
	(void*)&repl_kfree
};

static struct kedr_payload payload = {
	.mod 			= THIS_MODULE,
	.target_func_addrs 	= &target_func_addrs[0],
	.repl_func_addrs 	= &repl_func_addrs[0],
	.num_func_addrs		= ARRAY_SIZE(target_func_addrs)
};
/*********************************************************************/

static void
simple_payload_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
	printk(KERN_INFO "[simple_payload] Cleanup complete\n");
	return;
}

static int __init
simple_payload_init_module(void)
{
	BUG_ON(	ARRAY_SIZE(target_func_addrs) != 
		ARRAY_SIZE(repl_func_addrs));
	
	printk(KERN_INFO "[simple_payload] Initializing\n");
	return kedr_payload_register(&payload);
}

module_init(simple_payload_init_module);
module_exit(simple_payload_cleanup_module);
/*********************************************************************/
