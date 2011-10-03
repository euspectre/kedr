/*********************************************************************
 * Indicator: <$indicator.name$>
 *********************************************************************/
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("<$module.author$>");
MODULE_LICENSE("<$module.license$>");
/*********************************************************************/

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/debugfs.h>

#include <linux/mutex.h>

#include <kedr/fault_simulation/fault_simulation.h>
#include <kedr/control_file/control_file.h>

// Macros for unify output information to the kernel log file
#define debug(str, ...) pr_debug("%s: " str, __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) pr_err("%s: " str, __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

<$global: join(\n)$>

<$if concat(indicator.parameter.type)$>// Indicator parameters
struct point_data
{
    <$pointDataField : join(\n    )$>
};

<$endif$><$if isIndicatorState$>// Indicator variables
struct indicator_real_state
{
<$if concat(indicator.state.name)$>    <$stateVariableDeclaration: join(\n    )$>
<$endif$><$if concat(indicator.file.name)$>    <$controlFileDeclaration: join(\n    )$>
<$endif$>};

<$endif$>//Protect from concurrent access in getters and setter of files
DEFINE_MUTEX(indicator_mutex);

////////////////Auxiliary functions///////////////////////////

<$if concat(indicator.init.name)$><$indicatorInitDefinition : join()$>

<$endif$><$if concat(indicator.destroy.name)$><$indicatorDestroyDefinition : join()$>

<$endif$><$if concat(indicator.simulate.name)$><$indicatorSimulateDefinition : join()$>

<$endif$><$if concat(indicator.file.name)$><$controlFileFunctions : join()$>

<$endif$>//////////////Indicator's functions////////////////////////////
static int
indicator_simulate(void* state, void* user_data)
{
<$if concat(indicator.simulate.name)$>
    int result = 0;
<$if concat(indicator.simulate.first)$>    int never = 0;
<$endif$><$if concat(indicator.state.name)$>    <$indicatorStateDeclaration$> =
        (<$indicatorStateType$>)state;
<$endif$><$if concat(indicator.parameter.type)$>    struct point_data* point_data =
        (struct point_data*)user_data;
<$endif$><$indicatorSimulateCallFirst: join()$>
<$indicatorSimulateCall: join()$>
    return result;<$else$>return 0;<$endif$>
}

<$if isIndicatorDestroy$>static void
indicator_instance_destroy(void* state)
{
<$if isIndicatorState$>    <$indicatorStateDeclaration$> =
        (<$indicatorStateType$>)state;

<$endif$><$if concat(indicator.destroy.name)$>    // Call destroy functions
    <$indicatorDestroyCall : join(\n    )$>

<$endif$><$if concat(indicator.file.name)$>    // Destroy control files
    <$controlFileDestroy: join(\n     )$>

<$endif$><$if isIndicatorState$>    kfree(state);
<$endif$>}<$endif$>


<$if isIndicatorInit$>static int
indicator_instance_init(void** state,
    const char* params, struct dentry* control_directory)
{
<$if concat(indicator.init.name)$>    int error;<$else$><$if concat(indicator.file.name)$>    int error;<$endif$>
<$endif$>
<$if isIndicatorState$>    //Allocate state
    <$indicatorStateDeclaration$> = kzalloc(sizeof(*<$indicatorStateName$>), GFP_KERNEL);
	if(<$indicatorStateName$> == NULL)
    {
        pr_err("Cannot allocate memory for indicator state");
        return -ENOMEM;
    }
<$endif$>
<$if concat(indicator.init.name)$>    // Call init functions
<$indicatorInitCall : join(\n)$>
<$endif$><$if concat(indicator.file.name)$>    // Create control files
    if(control_directory != NULL)
    {
<$controlFileCreate: join(\n)$>
    }
<$endif$><$if isIndicatorState$>    *state = <$indicatorStateName$>;
<$endif$>    return 0;
<$if isFailInInit$>
fail:    // Call destroy on fail
    indicator_instance_destroy(<$if isIndicatorState$><$indicatorStateName$><$else$>NULL<$endif$>);
    return error;
<$endif$>}
<$endif$>

struct kedr_simulation_indicator* indicator;

static int __init
indicator_init(void)
{
    indicator = kedr_fsim_indicator_register("<$indicator.name$>",
        indicator_simulate,
        <$if concat(indicator.parameter.type)$>"<$indicator.parameter.type : join(,)$>"<$else$>""<$endif$>,
        <$if isIndicatorInit$>indicator_instance_init<$else$>NULL<$endif$>,
        <$if isIndicatorDestroy$>indicator_instance_destroy<$else$>NULL<$endif$>);
    if(indicator == NULL)
    {
        printk(KERN_ERR "Cannot register indicator.\n");
        return -1;
    }

    return 0;
}

static void
indicator_exit(void)
{
	kedr_fsim_indicator_unregister(indicator);
	return;
}

module_init(indicator_init);
module_exit(indicator_exit);
