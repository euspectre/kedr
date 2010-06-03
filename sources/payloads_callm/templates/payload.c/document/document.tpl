/*********************************************************************
 * Module: <$module.name$>
 *********************************************************************/
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("<$module.author$>");
MODULE_LICENSE("<$module.license$>");
/*********************************************************************/

#define CREATE_TRACE_POINTS
#include "trace_payload.h" /* trace event facilities */

#include <kedr/controller/controller_common.h>
<$header$>

/*********************************************************************
 * Replacement functions
 *********************************************************************/
<$block : join(\n\n)$>
/*********************************************************************/

/* Names and addresses of the functions of interest */
static void* target_func_addrs[] = {
<$targetFunctionAddress : join(,\n)$>
};

/* Addresses of the replacement functions */
static void* repl_func_addrs[] = {
<$replFunctionAddress : join(,\n)$>
};

static struct kedr_payload payload = {
	.mod 			= THIS_MODULE,
	.target_func_addrs 	= &target_func_addrs[0],
	.repl_func_addrs 	= &repl_func_addrs[0],
	.num_func_addrs		= ARRAY_SIZE(target_func_addrs)
};
/*********************************************************************/

static void
<$module.name$>_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
	printk(KERN_INFO "[<$module.name$>] Cleanup complete\n");
	return;
}

static int __init
<$module.name$>_init_module(void)
{
	BUG_ON(	ARRAY_SIZE(target_func_addrs) != 
		ARRAY_SIZE(repl_func_addrs));
	
	printk(KERN_INFO "[<$module.name$>] Initializing\n");
	return kedr_payload_register(&payload);
}

module_init(<$module.name$>_init_module);
module_exit(<$module.name$>_cleanup_module);
/*********************************************************************/
