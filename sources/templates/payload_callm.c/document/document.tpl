/*********************************************************************
 * Module: <$module.name$>
 *********************************************************************/
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("<$module.author$>");
MODULE_LICENSE("<$module.license$>");
/*********************************************************************/

#include <kedr/core/kedr.h>
#include <kedr/trace/trace.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$><$if concat(ellipsis)$>#include <stdarg.h>

<$endif$>
/*********************************************************************
 * Areas in the memory image of the target module (used to output 
 * addresses and offsets of the calls made by the module)
 *********************************************************************/
/* Start address and size of "core" area: .text, etc. */
static void *target_core_addr = NULL;
static unsigned int target_core_size = 0;

/* Start address and size of "init" area: .init.text, etc. */
static void *target_init_addr = NULL;
static unsigned int target_init_size = 0;

enum module_section_type {
	module_section_unknown = 0,
	module_section_init,
	module_section_core
};

/* 
 * According to absolute value of caller address ('abs_addr')
 * determine section, containing this address ('section_id'), and
 * relative address inside this section ('rel_addr').
 */

static void process_caller_address(void* abs_addr, int* section_id, ptrdiff_t* rel_addr)
{
	if((target_core_addr != NULL)
		&& (abs_addr >= target_core_addr)
		&& (abs_addr < target_core_addr + target_core_size))
	{
		*section_id = module_section_core;
		*rel_addr = abs_addr - target_core_addr;
	}
	else if((target_init_addr != NULL)
		&& (abs_addr >= target_init_addr)
		&& (abs_addr < target_init_addr + target_init_size))
	{
		*section_id = module_section_init;
		*rel_addr = abs_addr - target_init_addr;
	}
	else
	{
		*section_id = module_section_unknown;
		*rel_addr = abs_addr - (void*)0;
	}
}

/*********************************************************************
 * The callbacks to be called after the target module has just been
 * loaded and, respectively, when it is about to unload.
 *********************************************************************/
static void
target_load_callback(struct module *target_module)
{
	BUG_ON(target_module == NULL);

	target_core_addr = target_module->module_core;
	target_core_size = target_module->core_text_size;

	target_init_addr = target_module->module_init;
	target_init_size = target_module->init_text_size;
	
	kedr_trace_marker_target(target_module, THIS_MODULE, 1);
	
	return;
}

static void
target_unload_callback(struct module *target_module)
{
	kedr_trace_marker_target(target_module, THIS_MODULE, 0);
	
	BUG_ON(target_module == NULL);
	
	target_core_addr = NULL;
	target_core_size = 0;

	target_init_addr = NULL;
	target_init_size = 0;
	return;
}

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

	.target_load_callback   = target_load_callback,
	.target_unload_callback = target_unload_callback
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
