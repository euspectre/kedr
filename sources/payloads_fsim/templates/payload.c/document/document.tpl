/*********************************************************************
 * Module: <$module.name$>
 *********************************************************************/
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("<$module.author$>");
MODULE_LICENSE("<$module.license$>");
/*********************************************************************/

#include <kedr/base/common.h>

#include <linux/stacktrace.h>

<$if concat(fpoint.fault_code)$>#include <kedr/fault_simulation/fault_simulation.h>

<$endif$><$header$>

/* To minimize the unexpected consequences of trace event-related 
 * headers and symbols, place #include directives for system headers 
 * before '#define CREATE_TRACE_POINTS' directive
 */
#define CREATE_TRACE_POINTS
#include "trace_payload.h" /* trace event facilities */

/* 
 *   void get_caller_address(void* abs_addr, int section_id, ptrdiff_t rel_addr)
 *
 * Determine address of the caller for this replacement_function.
 *
 * All parameters should be lvalue.
 */

#define get_caller_address(abs_addr, section_id, rel_addr)      \
do {                                                            \
	abs_addr = __builtin_return_address(0);                     \
	if((target_core_addr != NULL)                               \
        && (abs_addr >= target_core_addr)                       \
        && (abs_addr < target_core_addr + target_core_size))    \
	{                                                           \
		section_id = 2;                                         \
		rel_addr = abs_addr - target_core_addr;                 \
	}                                                           \
	else if((target_init_addr != NULL)                          \
        && (abs_addr >= target_init_addr)                       \
        && (abs_addr < target_init_addr + target_init_size))    \
	{                                                           \
		section_id = 1;                                         \
		rel_addr = abs_addr - target_init_addr;                 \
	}                                                           \
	else                                                        \
	{                                                           \
		section_id = 0;                                         \
		rel_addr = abs_addr - (void*)0;                         \
	}                                                           \
}while(0)

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
    return;
}

static void
target_unload_callback(struct module *target_module)
{
    BUG_ON(target_module == NULL);
    
    target_core_addr = NULL;
    target_core_size = 0;

    target_init_addr = NULL;
    target_init_size = 0;
    return;
}

/*********************************************************************
 * Replacement functions
 *********************************************************************/
<$block : join(\n\n)$>
/*********************************************************************/

/* Names and addresses of the functions of interest */
static void* orig_addrs[] = {
<$targetFunctionAddress : join(,\n)$>
};

/* Addresses of the replacement functions */
static void* repl_addrs[] = {
<$replFunctionAddress : join(,\n)$>
};
<$if concat(fpoint.fault_code)$>//Array of simulation points and its parameters for register
struct sim_point_attributes
{
    struct kedr_simulation_point** p_point;
    const char* name;
    const char* format;
} sim_points[] = {
<$simPointAttributes : join(\n)$>
};
<$endif$>

static struct kedr_payload payload = {
    .mod                    = THIS_MODULE,
    .repl_table.orig_addrs  = &orig_addrs[0],
    .repl_table.repl_addrs  = &repl_addrs[0],
    .repl_table.num_addrs   = ARRAY_SIZE(orig_addrs),
    .target_load_callback   = target_load_callback,
    .target_unload_callback = target_unload_callback
};
/*********************************************************************/

static void
<$module.name$>_cleanup_module(void)
{
<$if concat(fpoint.fault_code)$>    int i;
<$endif$>    kedr_payload_unregister(&payload);
<$if concat(fpoint.fault_code)$>    for(i = 0; i < ARRAY_SIZE(sim_points); i++)
	{
		kedr_fsim_point_unregister(*sim_points[i].p_point);
	}<$endif$>
    
    KEDR_MSG("[<$module.name$>] Cleanup complete\n");
    return;
}

static int __init
<$module.name$>_init_module(void)
{
<$if concat(fpoint.fault_code)$>    int i;
<$endif$>    int result;

	BUILD_BUG_ON( ARRAY_SIZE(orig_addrs) != 
        ARRAY_SIZE(repl_addrs));

	KEDR_MSG("[<$module.name$>] Initializing\n");

<$if concat(fpoint.fault_code)$>	for(i = 0; i < ARRAY_SIZE(sim_points); i++)
	{
        *sim_points[i].p_point = kedr_fsim_point_register(sim_points[i].name,
            sim_points[i].format);
        if(*sim_points[i].p_point) break;
	}
	if(i != ARRAY_SIZE(sim_points))
	{
		KEDR_MSG("[<$module.name$>] Failed to register simulation points\n");
		for(--i; i>=0 ; i--)
		{
            kedr_fsim_point_unregister(*sim_points[i].p_point);
        }
        return -1;
	}

<$endif$>    result = kedr_payload_register(&payload);
<$if concat(fpoint.fault_code)$>    if(result)
    {
		for(--i; i>=0 ; i--)
		{
            kedr_fsim_point_unregister(*sim_points[i].p_point);
        }
        return result;
    }<$else$>    if(result) return result;
<$endif$>
	return 0;
}

module_init(<$module.name$>_init_module);
module_exit(<$module.name$>_cleanup_module);
/*********************************************************************/
