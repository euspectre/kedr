/*********************************************************************
 * Module: <$module.name$>
 *********************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>

MODULE_AUTHOR("<$module.author$>");
MODULE_LICENSE("<$module.license$>");
/*********************************************************************/

/* Workaround for mainline commit
 * ac6424b981bc ("sched/wait: Rename wait_queue_t => wait_queue_entry_t") */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
# define wait_queue_t wait_queue_entry_t
#endif

#include <kedr/core/kedr.h>
#include <kedr/trace/trace.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$><$if concat(ellipsis)$>#include <stdarg.h>

<$endif$>

/*********************************************************************
 * Interception functions
 *********************************************************************/
<$if concat(function.name)$><$block : join(\n\n)$>
<$endif$>/*********************************************************************/

static struct kedr_post_pair post_pairs[] =
{
<$postPair_comma: join()$>	{
		.orig = NULL
	}
};

static struct kedr_pre_pair pre_pairs[] =
{
<$prePair_comma: join()$>	{
		.orig = NULL
	}
};


static struct kedr_payload payload = {
	.mod                    = THIS_MODULE,

	.post_pairs				= post_pairs,
	.pre_pairs				= pre_pairs,
};
/*********************************************************************/

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void __exit
<$module.name$>_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
	kedr_trace_pp_unregister();
	
	functions_support_unregister();
}

static int __init
<$module.name$>_init_module(void)
{
	int result;

	result = functions_support_register();
	if(result) return result;
	
	result = kedr_payload_register(&payload);
	if(result)
	{
		functions_support_unregister();
		return result;
	}
	
	return 0;
}

module_init(<$module.name$>_init_module);
module_exit(<$module.name$>_cleanup_module);
/*********************************************************************/
