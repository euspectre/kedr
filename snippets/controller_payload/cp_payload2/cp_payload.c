#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
//#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */

#include <asm/uaccess.h>	/* copy_*_user */

#include <cp_controller/controller_common.h>

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");

/* ================================================================ */
/* Declarations of replacement functions (should be the same as for 
 * the target functions but with a different name.) 
 * */
static long 
repl_copy_from_user(void* to, const void __user * from, unsigned long n);

static long 
repl_copy_to_user(void __user * to, const void* from, unsigned long n);
/* ================================================================ */

/* Names and addresses of the functions of interest */
static void* target_func_addrs[] = {
/* An ugly and fragile but working way to get around copy_from_user() being 
 * inline on newer systems */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
	(void*)&copy_from_user,
#else
	(void*)&_copy_from_user,
#endif
	(void*)&copy_to_user
};

/* Addresses of the replacement functions */
static void* repl_func_addrs[] = {
	(void*)&repl_copy_from_user,
	(void*)&repl_copy_to_user
};

static struct kedr_payload payload = {
	.mod 			= THIS_MODULE,
	.target_func_addrs 	= &target_func_addrs[0],
	.repl_func_addrs 	= &repl_func_addrs[0],
	.num_func_addrs		= ARRAY_SIZE(target_func_addrs)
};
/* ================================================================ */

static void
cfake_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
	printk(KERN_INFO "[cp_payload2] Cleanup successful\n");
	return;
}

static int __init
cfake_init_module(void)
{
	BUG_ON(	ARRAY_SIZE(target_func_addrs) != 
		ARRAY_SIZE(repl_func_addrs));
	
	printk(KERN_INFO "[cp_payload2] Initializing\n");
	return kedr_payload_register(&payload);
}

module_init(cfake_init_module);
module_exit(cfake_cleanup_module);
/* ================================================================ */

/* Definitions of replacement functions
 */
static long 
repl_copy_from_user(void* to, const void __user * from, unsigned long n)
{
	long result = copy_from_user(to, from, n);
	printk(	KERN_INFO "[cp_payload2] Called: "
		"copy_from_user(%p, %p, %lu), result: %ld\n",
		to,
		from,
		n,
		result
	);
	return result;
}

static long 
repl_copy_to_user(void __user * to, const void* from, unsigned long n)
{
	long result = copy_to_user(to, from, n);
	printk(	KERN_INFO "[cp_payload2] Called: "
		"copy_to_user(%p, %p, %lu), result: %ld\n",
		to,
		from,
		n,
		result
	);
	return result;
}
/* ================================================================ */

