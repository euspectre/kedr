#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */

#include <kedr/fault_simulation/fsim_base.h>
#include <kedr/fault_simulation/fsim_indicator_manager.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static char str_result[100] = "";
module_param_string(result, str_result, sizeof(str_result), S_IRUGO);

#define SET_RESULT(str, ...) snprintf(str_result, sizeof(str_result), str, __VA_ARGS__)
#define SET_RESULT0(str) SET_RESULT("%s", str)

static const char* point_name="point name";
static const char* indicator_name="my indicator";


static int indicator_2_calls(void* indicator_state, void* user_data)
{
    int* n_calls = (int*)indicator_state;
    (void)user_data;

    (*n_calls)++;
    return *n_calls >= 2;
}
static int indicator_2_calls_state_init(const void* params, size_t params_len, void** state)
{
    int* state_real;
    (void) params;
    (void)params_len;

    state_real = kmalloc(sizeof(*state_real), GFP_KERNEL);
    if(state_real == NULL)
    {
        printk(KERN_ERR "Cannot allocate memory for state.\n");
        return 1;
    }
    *state_real = 0;
    *state = state_real;
    return 0;
}

static void indicator_2_calls_state_delete(void* state)
{
    kfree(state);
}

static int __init
module_this_init(void)
{
    int tmp;
    struct kedr_simulation_point* point = kedr_fsim_point_register(point_name, NULL);
    if(point == NULL)
    {
        SET_RESULT0("Cannot register simulation point");
        return 0;
    }
    if(kedr_fsim_indicator_function_register(indicator_name,
        indicator_2_calls, NULL, THIS_MODULE,
        indicator_2_calls_state_init,
        indicator_2_calls_state_delete))
    {
        SET_RESULT0("Cannot register indicator function.");
        kedr_fsim_point_unregister(point);
        return 0;
    }
    if(kedr_fsim_set_indicator_by_name(point_name, indicator_name, NULL, 0))
    {
        SET_RESULT0("Cannot set indicator for point");
        kedr_fsim_indicator_function_unregister(indicator_name);
        kedr_fsim_point_unregister(point);
        return 0;
    }
    if((tmp = kedr_fsim_simulate(point, NULL)))
    {
        SET_RESULT("First simulation return %d, but should return 0.", tmp);
        kedr_fsim_indicator_function_unregister(indicator_name);
        kedr_fsim_point_unregister(point);
        return 0;
    }
    if(!kedr_fsim_simulate(point, NULL))
    {
        SET_RESULT0("Second simulation return 0, but should return not 0.");
        kedr_fsim_indicator_function_unregister(indicator_name);
        kedr_fsim_point_unregister(point);
        return 0;
    }
    if(kedr_fsim_indicator_clear(point_name))
    {
        SET_RESULT0("Failed to clear indicator for the point.");
        kedr_fsim_indicator_function_unregister(indicator_name);
        kedr_fsim_point_unregister(point);
        return 0;
    }
    if((tmp=kedr_fsim_indicator_clear(point_name)))
    {
        SET_RESULT("Simulation after clearing indicator return %d, but should 0.", tmp);
        kedr_fsim_indicator_function_unregister(indicator_name);
        kedr_fsim_point_unregister(point);
        return 0;
    }
    kedr_fsim_indicator_function_unregister(indicator_name);
    kedr_fsim_point_unregister(point);
    SET_RESULT0("Ok");
    return 0;
}

static void
module_this_exit(void)
{
	return;
}

module_init(module_this_init);
module_exit(module_this_exit);
