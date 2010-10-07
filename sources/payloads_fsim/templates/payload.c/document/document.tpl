/*********************************************************************
 * Module: <$module.name$>
 *********************************************************************/
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("<$module.author$>");
MODULE_LICENSE("<$module.license$>");
/*********************************************************************/

#include <kedr/base/common.h>

#include <kedr/fault_simulation/fault_simulation.h>

<$header$>

/* To minimize the unexpected consequences of trace event-related 
 * headers and symbols, place #include directives for system headers 
 * before '#define CREATE_TRACE_POINTS' directive
 */
#define CREATE_TRACE_POINTS
#include "trace_payload.h" /* trace event facilities */

<$if concat(fpoint.reuse_point)$>
/*
 * For reusing simulation points
 * 
 * If replacement function reuse previousely declared simulation point, it define its point as fake point.
 * Otherwise, it should declare point variable.
 */
static struct kedr_simulation_point* fake_fsim_point;
<$endif$>


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
//Arrays of simulation points and its parameters for register
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


static struct kedr_payload payload = {
    .mod                    = THIS_MODULE,
    .repl_table.orig_addrs  = &orig_addrs[0],
    .repl_table.repl_addrs  = &repl_addrs[0],
    .repl_table.num_addrs   = ARRAY_SIZE(orig_addrs)
};
/*********************************************************************/

static void
<$module.name$>_cleanup_module(void)
{
    int i;
    for(i = 0; i < ARRAY_SIZE(sim_points); i++)
	{
		unregister_point_i(i);
	}
    kedr_payload_unregister(&payload);
    KEDR_MSG("[<$module.name$>] Cleanup complete\n");
    return;
}

static int __init
<$module.name$>_init_module(void)
{
    int i;
	int result;

	BUILD_BUG_ON( ARRAY_SIZE(orig_addrs) != 
        ARRAY_SIZE(repl_addrs));
    BUILD_BUG_ON( ARRAY_SIZE(sim_points) !=
		ARRAY_SIZE(sim_point_names));
	BUILD_BUG_ON( ARRAY_SIZE(sim_points) !=
		ARRAY_SIZE(sim_point_formats));


	KEDR_MSG("[<$module.name$>] Initializing\n");

    result = kedr_payload_register(&payload);
	if(result) return result;

	for(i = 0; i < ARRAY_SIZE(sim_points); i++)
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
        kedr_payload_unregister(&payload);
        return -1;
	}
	return 0;
}

module_init(<$module.name$>_init_module);
module_exit(<$module.name$>_cleanup_module);
/*********************************************************************/
