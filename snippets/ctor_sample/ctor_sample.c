#include <linux/module.h>
/*#include <linux/moduleparam.h>*/
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/errno.h>	/* error codes */

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("Dual BSD/GPL");

/* ================================================================ */
/* Ctors and dtors - just to check if they are called and in what order */
 
__attribute__((constructor))
void
my_ctor2(void)
{
	printk(KERN_NOTICE "[ctor_sample] my_ctor2() called\n");
	return;
}
 
__attribute__((constructor))
void
my_ctor1(void)
{
	printk(KERN_NOTICE "[ctor_sample] my_ctor1() called\n");
	return;
}
 
__attribute__((destructor))
void
my_dtor1(void)
{
	printk(KERN_NOTICE "[ctor_sample] my_dtor1() called\n");
	return;
}
 
__attribute__((destructor))
void
my_dtor3(void)
{
	printk(KERN_NOTICE "[ctor_sample] my_dtor1() called\n");
	return;
}
 
__attribute__((destructor))
void
my_dtor2(void)
{
	printk(KERN_NOTICE "[ctor_sample] my_dtor1() called\n");
	return;
}

/* ================================================================ */
static void
ctor_sample_cleanup_module(void)
{
	printk(KERN_ALERT "[ctor_sample] Cleaning up\n");
	return;
}

static int __init
ctor_sample_init_module(void)
{
	printk(KERN_ALERT "[ctor_sample] Initializing\n");
	return 0; /* success */
}

module_init(ctor_sample_init_module);
module_exit(ctor_sample_cleanup_module);
