/*********************************************************************
 * Module: <$module.name$>
 *********************************************************************/
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("<$module.author$>");
MODULE_LICENSE("<$module.license$>");
/*********************************************************************/

#include <kedr/core/kedr.h>

#include <kedr/fault_simulation/fault_simulation.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$>/*********************************************************************
 * Replacement functions
 *********************************************************************/
<$if concat(function.name)$><$block : join(\n\n)$>
<$endif$>/*********************************************************************/

/* Replace pairs */
static struct kedr_replace_pair replace_pairs[] =
{
<$replacePair_comma: join()$>
	{
		.orig = NULL
	}
};

//Array of simulation points and its parameters for register
struct sim_point_attributes
{
    struct kedr_simulation_point** p_point;
    const char* name;
    const char* format;
} sim_points[] = {
<$if concat(function.name)$><$simPointAttributes : join(\n)$>
<$endif$>	{NULL}
};

static struct kedr_payload payload = {
    .mod                    = THIS_MODULE,

	.replace_pairs 			= replace_pairs,
};
/*********************************************************************/
extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void
<$module.name$>_cleanup_module(void)
{
	struct sim_point_attributes* sim_point;

	kedr_payload_unregister(&payload);
	for(sim_point = sim_points; sim_point->p_point != NULL; sim_point++)
	{
		kedr_fsim_point_unregister(*sim_point->p_point);
	}
    functions_support_unregister();	
}

static int __init
<$module.name$>_init_module(void)
{
	struct sim_point_attributes* sim_point;
    int result;

	result = functions_support_register();
	if(result) return result;

	for(sim_point = sim_points; sim_point->p_point != NULL; sim_point++)
	{
        *sim_point->p_point = kedr_fsim_point_register(sim_point->name,
            sim_point->format);
        if(*sim_point->p_point == NULL)
		{
			pr_err("Failed to register simulation point %s\n",
				sim_point->name);
			result = -EINVAL;
			goto sim_points_err;
		}
	}

    result = kedr_payload_register(&payload);
	if(result) goto payload_err;

	return 0;

payload_err:
sim_points_err:
	for(--sim_point; (sim_point - sim_points) >= 0 ; sim_point--)
	{
		kedr_fsim_point_unregister(*sim_point->p_point);
	}

	return result;
}

module_init(<$module.name$>_init_module);
module_exit(<$module.name$>_cleanup_module);
/*********************************************************************/
