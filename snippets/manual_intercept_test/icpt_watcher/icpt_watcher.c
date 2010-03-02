/* icpt_watcher */
/* This module contains the functions to be called from the target module 
instead of __kmalloc(). One of the replacement functions simply records 
information, the other one simulates __kmalloc() failure.
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("Dual BSD/GPL");

/* ================================================================ */
static void
icpt_watcher_cleanup_module(void)
{
	printk(KERN_ALERT "[icpt_watcher] Cleaning up\n");
	return;
}

static int __init
icpt_watcher_init_module(void)
{
	printk(KERN_ALERT "[icpt_watcher] Initializing\n");
	return 0; /* success */
}

module_init(icpt_watcher_init_module);
module_exit(icpt_watcher_cleanup_module);
/* ================================================================ */

/* A wrapper around __kmalloc() that just records the arguments, calls 
__kmalloc, records and returns its result.
*/
void* 
icpt_watcher_kmalloc(size_t size, gfp_t flags)
{
	void* ret = NULL;
	printk(KERN_ALERT 
		"[icpt_watcher] __kmalloc() called with size = %lu, flags = %lu\n",
		(unsigned long)size, (unsigned long)flags);
	ret = __kmalloc(size, flags);
	printk(KERN_ALERT 
		"[icpt_watcher] __kmalloc() returned %p\n",
		ret);
	return ret;
}

/* A wrapper around __kmalloc() that simulates its failure.
*/
void* 
icpt_watcher_kmalloc_fail(size_t size, gfp_t flags)
{
	printk(KERN_ALERT 
		"[icpt_watcher] __kmalloc() called with size = %lu, flags = %lu\n",
		(unsigned long)size, (unsigned long)flags);
	printk(KERN_ALERT 
		"[icpt_watcher] simulating __kmalloc() failure\n");
	return NULL;
}

/* ================================================================ */
