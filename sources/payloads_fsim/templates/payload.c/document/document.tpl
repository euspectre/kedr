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

<$if concat(fpoint.fault_code)$>#include <kedr/fault_simulation/fault_simulation.h><$endif$>

<$header$>

/* To minimize the unexpected consequences of trace event-related 
 * headers and symbols, place #include directives for system headers 
 * before '#define CREATE_TRACE_POINTS' directive
 */
#define CREATE_TRACE_POINTS
#include "trace_payload.h" /* trace event facilities */

<$if concat(fpoint.fault_code)$><$if concat(fpoint.reuse_point)$>
/*
 * For reusing simulation points
 * 
 * If replacement function reuse previousely declared simulation point, it define its point as fake point.
 * Otherwise, it should declare point variable.
 */
static struct kedr_simulation_point* fake_fsim_point;
<$endif$><$endif$>

/* 
 *   void get_caller_address(void* abs_addr, int section_id, ptrdiff_t rel_addr)
 *
 * Determine address of the caller for this replacement_function.
 *
 * All parameters should be lvalue.
 */
#if defined(CONFIG_STACKTRACE) && defined(CONFIG_FRAME_POINTER)

#define get_caller_address(abs_addr, section_id, rel_addr)      \
do {                                                            \
    unsigned long stack_entry;                                  \
	struct stack_trace trace = {                                \
		.nr_entries = 0,                                        \
		.entries = &stack_entry,                                \
		.max_entries = 1,                                       \
		.skip = 2                                               \
	};                                                          \
    save_stack_trace(&trace);                                   \
	abs_addr = (void*)stack_entry;                              \
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
#else /* if defined(CONFIG_STACKTRACE) && defined(CONFIG_FRAME_POINTER)*/
#define get_caller_address(abs_addr, section_id, rel_addr)      \
do {                                                            \
    abs_addr = 0;                                            \
    section_id = 3;                                             \
    rel_addr = 0;                                               \
}while(0)
#endif /* if defined(CONFIG_STACKTRACE) && defined(CONFIG_FRAME_POINTER)*/

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
<$if concat(fpoint.fault_code)$>//Arrays of simulation points and its parameters for register
static struct kedr_simulation_point** sim_points[] = {
<$simPointAddress : join(,\n)$>
};

static const char* sim_point_names[] = {
<$simPointName : join(,\n)$>
};

static const char* sim_point_formats[] = {
<$simPointFormat : join(,\n)$>
};

//return not 0 if registration of simulation point i is failed
static int register_point_i(int i)
{
<$if concat(fpoint.reuse_point)$>
    if(sim_points[i] == &fake_fsim_point) return 0;//needn't to register
<$endif$>        
    *(sim_points[i]) = kedr_fsim_point_register(sim_point_names[i],
        (sim_point_formats[i]));
    return *(sim_points[i]) == NULL;
}

static void unregister_point_i(int i)
{
<$if concat(fpoint.reuse_point)$>
    if(sim_points[i] == &fake_fsim_point) return;//needn't to unregister
<$endif$>        
    kedr_fsim_point_unregister(*(sim_points[i]));
}
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
    for(i = 0; i < ARRAY_SIZE(sim_points); i++)
	{
		unregister_point_i(i);
	}<$endif$>
    kedr_payload_unregister(&payload);
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
<$if concat(fpoint.fault_code)$>    BUILD_BUG_ON( ARRAY_SIZE(sim_points) !=
		ARRAY_SIZE(sim_point_names));
	BUILD_BUG_ON( ARRAY_SIZE(sim_points) !=
		ARRAY_SIZE(sim_point_formats));<$endif$>

	KEDR_MSG("[<$module.name$>] Initializing\n");

<$if concat(fpoint.fault_code)$>	for(i = 0; i < ARRAY_SIZE(sim_points); i++)
	{
        if(register_point_i(i)) break;
	}
	if(i != ARRAY_SIZE(sim_points))
	{
		KEDR_MSG("[<$module.name$>] Failed to register simulation points\n");
		for(--i; i>=0 ; i--)
		{
            unregister_point_i(i);
        }
        return -1;
	}

<$endif$>    result = kedr_payload_register(&payload);
<$if concat(fpoint.fault_code)$>    if(result)
    {
		for(--i; i>=0 ; i--)
		{
            unregister_point_i(i);
        }
        return result;
    }<$else$>    if(result) return result;
<$endif$>
	return 0;
}

module_init(<$module.name$>_init_module);
module_exit(<$module.name$>_cleanup_module);
/*********************************************************************/
