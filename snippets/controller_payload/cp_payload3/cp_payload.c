#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
//#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */

//#include <asm/uaccess.h>	/* copy_*_user */
#include <linux/semaphore.h>	/* down_* & up */

#include <cp_controller/controller_common.h>

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");

/* ================================================================ */
/* Declarations of replacement functions (should be the same as for 
 * the target functions but with a different name.) 
 * */
static int 
repl_down_interruptible(struct semaphore *sem);

static void 
repl_up(struct semaphore *sem);
/* ================================================================ */

/* Names and addresses of the functions of interest */
static void* target_func_addrs[] = {
	(void*)&down_interruptible,
	(void*)&up
};

/* Addresses of the replacement functions */
static void* repl_func_addrs[] = {
	(void*)&repl_down_interruptible,
	(void*)&repl_up
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
	printk(KERN_INFO "[cp_payload3] Cleanup successful\n");
	return;
}

static int __init
cfake_init_module(void)
{
	BUG_ON(	ARRAY_SIZE(target_func_addrs) != 
		ARRAY_SIZE(repl_func_addrs));
	
	printk(KERN_INFO "[cp_payload3] Initializing\n");
	return kedr_payload_register(&payload);
}

module_init(cfake_init_module);
module_exit(cfake_cleanup_module);
/* ================================================================ */

/* Definitions of replacement functions
 */
static int 
repl_down_interruptible(struct semaphore *sem)
{
	int result = down_interruptible(sem);
	printk(	KERN_INFO "[cp_payload3] Called: "
		"down_interruptible(%p), result: %d\n",
		sem,
		result
	);
	return result;
}

static void 
repl_up(struct semaphore *sem)
{
	up(sem);
	printk(	KERN_INFO "[cp_payload3] Called: "
		"up(%p)\n",
		sem
	);
	return;
}
/* ================================================================ */

