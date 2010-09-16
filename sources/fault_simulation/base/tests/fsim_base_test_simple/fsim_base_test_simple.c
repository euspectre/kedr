#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */

#include <kedr/fault_simulation/fsim_base.h>

#define point_name "tested point"

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static char result[50] = "";
module_param_string(result, result, sizeof(result), S_IRUGO);

static int indicator_return_5(void* indicator_state, void* user_data)
{
    return 5;
}

static int __init
fsim_base_test_simple_init(void)
{
    int fsim_result;
    struct kedr_simulation_point* point = kedr_fsim_point_register(point_name, NULL);
    if(point == NULL)
    {
        strncpy(result, "Cannot create simulation point", sizeof(result));
        return 0;
    }
    fsim_result = kedr_fsim_simulate(point, NULL);
    if(fsim_result)
    {
        snprintf(result, sizeof(result), "Simulation point without indicator set return %d(should be 0).",
            fsim_result);
        kedr_fsim_point_unregister(point);
        return 0;
    }
    
    if(kedr_fsim_indicator_set(point_name, indicator_return_5, NULL,
        THIS_MODULE, NULL, NULL))
    {
        snprintf(result, sizeof(result), "Failed to set indicator for the point.");
        kedr_fsim_point_unregister(point);
        return 0;
    }
    
    fsim_result = kedr_fsim_simulate(point, NULL);
    if(fsim_result != 5)
    {
        snprintf(result, sizeof(result), "Simulation point with indicator set return %d(should be 5).",
            fsim_result);
        kedr_fsim_point_unregister(point);
        return 0;
    }

    kedr_fsim_point_unregister(point);
    strncpy(result, "Ok", sizeof(result));

    return 0;
}

static void
fsim_base_test_simple_exit(void)
{
	return;
}

module_init(fsim_base_test_simple_init);
module_exit(fsim_base_test_simple_exit);