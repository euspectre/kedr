#include <linux/module.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() (for our needs) */
//previous header already include this
//#include <linux/gfp.h>      /* gfp_flags constants*/

#include <kedr/fault_simulation/fault_simulation.h>
#include "calculator/calculator.h"

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static const struct kedr_calc_const gfp_flags_const[] =
{
    KEDR_C_CONSTANT(GFP_NOWAIT),//
    KEDR_C_CONSTANT(GFP_NOIO),//
    KEDR_C_CONSTANT(GFP_NOFS),//
    KEDR_C_CONSTANT(GFP_KERNEL),//
    KEDR_C_CONSTANT(GFP_TEMPORARY),//
    KEDR_C_CONSTANT(GFP_USER),//
    KEDR_C_CONSTANT(GFP_HIGHUSER),//
    KEDR_C_CONSTANT(GFP_HIGHUSER_MOVABLE),//
//    KEDR_C_CONSTANT(GFP_IOFS),//
//may be others gfp_flags
};

static const struct kedr_calc_const_vec indicator_kmalloc_constants =
{
    .n_elems=sizeof(gfp_flags_const) / sizeof(gfp_flags_const[0]),//
    .elems=gfp_flags_const,
    
};

static const char* var_names[]= {"size", "flags"};


const char* indicator_kmalloc_name = "sample_indicator_kmalloc";
//According to convensions of 'format_string' of fault simulation
struct point_data
{
    size_t size;
    gfp_t flags;
};
//
struct indicator_kmalloc_state
{
    kedr_calc_t* calc;
};

static int indicator_kmalloc_simulate(void* indicator_state, void* user_data)
{
    //printk(KERN_INFO "Indicator was called.\n");
    struct point_data* point_data =
        (struct point_data*)user_data;
    struct indicator_kmalloc_state* indicator_kmalloc_state =
        (struct indicator_kmalloc_state*)indicator_state;
    
    kedr_calc_int_t vars[2];
    vars[0] = (kedr_calc_int_t)point_data->size;
    vars[1] = (kedr_calc_int_t)point_data->flags;
    
    return kedr_calc_evaluate(indicator_kmalloc_state->calc, vars);
}

static void indicator_kmalloc_instance_destroy(void* indicator_state)
{
    struct indicator_kmalloc_state* indicator_kmalloc_state =
        (struct indicator_kmalloc_state*)indicator_state;
    
    kedr_calc_delete(indicator_kmalloc_state->calc);
    kfree(indicator_kmalloc_state);
}

static int indicator_kmalloc_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory)
{
    struct indicator_kmalloc_state* indicator_kmalloc_state;
    (void)control_directory;//not used

    indicator_kmalloc_state = kmalloc(sizeof(*indicator_kmalloc_state), GFP_KERNEL);
    if(indicator_kmalloc_state == NULL)
    {
        printk(KERN_ERR "indicator_kmalloc_instance_init: Cannot allocate indicator state.\n");
        return -1;
    }
    if(params != NULL && *params != '\0')
    {
        indicator_kmalloc_state->calc = kedr_calc_parse(params,
            1, &indicator_kmalloc_constants,
            sizeof(var_names)/sizeof(var_names[0]), var_names);
        if(indicator_kmalloc_state->calc == NULL)
        {
            printk(KERN_ERR "indicator_kmalloc_instance_init: "
                "Cannot parse string expression.\n");
        }
    }
    else
    {
        indicator_kmalloc_state->calc = kedr_calc_parse("0", 0, NULL, 0, NULL);
        if(indicator_kmalloc_state->calc == NULL)
        {
            printk(KERN_ERR "indicator_kmalloc_instance_init: "
                "Cannot parse string expression '0'(probably, insufficient memory).\n");
        }
    }
    if(indicator_kmalloc_state->calc == NULL)
    {
        kfree(indicator_kmalloc_state);
        return -1;
    }
    *indicator_state = indicator_kmalloc_state;
    
    return 0;
}

struct kedr_simulation_indicator* indicator_kmalloc;

static int __init
indicator_kmalloc_init(void)
{
    indicator_kmalloc = kedr_fsim_indicator_register(indicator_kmalloc_name,
        indicator_kmalloc_simulate, "size_t,gfp_flags",
        indicator_kmalloc_instance_init,
        indicator_kmalloc_instance_destroy);
    if(indicator_kmalloc == NULL)
    {
        printk(KERN_ERR "Cannot register indicator.\n");
        return 1;
    }

    return 0;
}

static void
indicator_kmalloc_exit(void)
{
	kedr_fsim_indicator_unregister(indicator_kmalloc);
	return;
}

module_init(indicator_kmalloc_init);
module_exit(indicator_kmalloc_exit);