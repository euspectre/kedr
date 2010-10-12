#include <linux/module.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() (for our needs) */
//previous header already include this
//#include <linux/gfp.h>      /* gfp_flags constants*/

#include <linux/debugfs.h>

#include <linux/mutex.h>

#include <kedr/fault_simulation/fault_simulation.h>
#include "calculator.h"
#include "control_file.h"

#include <kedr/base/common.h> /* in_init */
#include <linux/sched.h> /* task_pid */


// Macros for unify output information to the kernel log file
#define debug(str, ...) pr_debug("%s: " str, __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) pr_err("%s: " str, __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)


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
    .n_elems = ARRAY_SIZE(gfp_flags_const),
    .elems   = gfp_flags_const,
};

static const char* var_names[]= {
    "size",
    "flags",
    "times",//local variable of indicator state
};

kedr_calc_int_t pid_weak_var_compute(void)
{
    return (kedr_calc_int_t)task_pid(current);
}
kedr_calc_int_t in_init_weak_var_compute(void)
{
    return (kedr_calc_int_t)kedr_target_module_in_init();
}

static const struct kedr_calc_weak_var weak_vars[] = {
    { .name = "PID", .compute = pid_weak_var_compute },
    { .name = "in_init", .compute = in_init_weak_var_compute }
};

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
    char* expression;
    atomic_t times;
    struct dentry* expression_file;
};
//Protect from concurrent access all except read from 'struct indicator_kmalloc_state'.'calc'.
// That read is protected by rcu.
DEFINE_MUTEX(indicator_mutex);

////////////////Auxiliary functions///////////////////////////

// Initialize expression part of the indicator state.
static int
indicator_state_expression_init(struct indicator_kmalloc_state* state, const char* expression);
// Cange expression of the indicator. On fail state is not changed.
// Should be executed with mutex taken.
static int
indicator_state_expression_set_internal(struct indicator_kmalloc_state* state, const char* expression);
// Destroy expression part of the indicator state
static void
indicator_state_expression_destroy(struct indicator_kmalloc_state* state);
//
static int indicator_create_expression_file(struct indicator_kmalloc_state* state, struct dentry* dir);
static void indicator_remove_expression_file(struct indicator_kmalloc_state* state);
//
//////////////Indicator's functions declaration////////////////////////////

static int indicator_kmalloc_simulate(void* indicator_state, void* user_data);
static void indicator_kmalloc_instance_destroy(void* indicator_state);
static int indicator_kmalloc_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory);

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

////////////Implementation of indicator's functions////////////////////

int indicator_kmalloc_simulate(void* indicator_state, void* user_data)
{
    int result;
    //printk(KERN_INFO "Indicator was called.\n");
    struct point_data* point_data =
        (struct point_data*)user_data;
    struct indicator_kmalloc_state* indicator_kmalloc_state =
        (struct indicator_kmalloc_state*)indicator_state;
    
    
    kedr_calc_int_t vars[3];
    vars[0] = (kedr_calc_int_t)point_data->size;
    vars[1] = (kedr_calc_int_t)point_data->flags;
    vars[2] = atomic_inc_return(&indicator_kmalloc_state->times);

    BUILD_BUG_ON(ARRAY_SIZE(vars) != ARRAY_SIZE(var_names));
    
    rcu_read_lock();
    result = kedr_calc_evaluate(rcu_dereference(indicator_kmalloc_state->calc), vars);
    rcu_read_unlock();

    return result;
}

void indicator_kmalloc_instance_destroy(void* indicator_state)
{
    struct indicator_kmalloc_state* indicator_kmalloc_state =
        (struct indicator_kmalloc_state*)indicator_state;
    
    indicator_remove_expression_file(indicator_kmalloc_state);
    indicator_state_expression_destroy(indicator_kmalloc_state);
    
    kfree(indicator_kmalloc_state);
}

int indicator_kmalloc_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory)
{
    struct indicator_kmalloc_state* indicator_kmalloc_state;
    const char* expression = (*params != '\0') ? params : "0";
    indicator_kmalloc_state = kmalloc(sizeof(*indicator_kmalloc_state), GFP_KERNEL);
    if(indicator_kmalloc_state == NULL)
    {
        pr_err("Cannot allocate indicator state.");
        return -1;
    }
    if(indicator_state_expression_init(indicator_kmalloc_state, expression))
    {
        kfree(indicator_kmalloc_state);
        return -1;
    }
    if(indicator_create_expression_file(indicator_kmalloc_state, control_directory))
    {
        indicator_state_expression_destroy(indicator_kmalloc_state);
        kfree(indicator_kmalloc_state);
        return -1;
    }
    *indicator_state = indicator_kmalloc_state;
    return 0;
}
//////////////////Implementation of auxiliary functions////////////////
// Initialize expression part of the indicator state.
static int
indicator_state_expression_init(struct indicator_kmalloc_state* state, const char* expression)
{
    state->calc = kedr_calc_parse(expression,
        1, &indicator_kmalloc_constants,
        ARRAY_SIZE(var_names), var_names,
        ARRAY_SIZE(weak_vars), weak_vars);
    if(state->calc == NULL)
    {
        pr_err("Cannot parse string expression.");
        return -1;
    }
    state->expression = kmalloc(strlen(expression) + 1, GFP_KERNEL);
    if(state->expression == NULL)
    {
        pr_err("Cannot allocate memory for string expression.");
        kedr_calc_delete(state->calc);
        return -1;
    }
    strcpy(state->expression, expression);
    atomic_set(&state->times, 0);
    return 0;

}
// Cange expression of the indicator. On fail state is not changed.
// Should be executed with mutex taken.
static int
indicator_state_expression_set_internal(struct indicator_kmalloc_state* state, const char* expression)
{
    char *new_expression;
    kedr_calc_t *old_calc, *new_calc;
    
    new_calc = kedr_calc_parse(expression,
        1, &indicator_kmalloc_constants,
        ARRAY_SIZE(var_names), var_names,
        ARRAY_SIZE(weak_vars), weak_vars);
    if(new_calc == NULL)
    {
        pr_err("Cannot parse expression");
        return -EINVAL;
    }
    
    new_expression = kmalloc(strlen(expression) + 1, GFP_KERNEL);
    if(new_expression == NULL)
    {
        pr_err("Cannot allocate memory for string expression.");
        kedr_calc_delete(new_calc);
        return -1;
    }
    strcpy(new_expression, expression);
    
    old_calc = state->calc;
    atomic_set(&state->times, 0);
    
    rcu_assign_pointer(state->calc, new_calc);
    
    kfree(state->expression);
    state->expression = new_expression;
    
    synchronize_rcu();
    kedr_calc_delete(old_calc);
    
    return 0;
}
// Destroy expression part of the indicator state
static void
indicator_state_expression_destroy(struct indicator_kmalloc_state* state)
{
    kedr_calc_delete(state->calc);
    kfree(state->expression);
}

/////////////////////Files implementation/////////////////////////////
static char* indicator_expression_file_get_str(struct inode* inode);
static int indicator_expression_file_set_str(const char* str, struct inode* inode);

CONTROL_FILE_OPS(indicator_expression_file_operations,
    indicator_expression_file_get_str, indicator_expression_file_set_str);

int indicator_create_expression_file(struct indicator_kmalloc_state* state, struct dentry* dir)
{
    state->expression_file = debugfs_create_file("expression",
        S_IRUGO | S_IWUSR | S_IWGRP,
        dir,
        state, &indicator_expression_file_operations);
    if(state->expression_file == NULL)
    {
        pr_err("Cannot create expression file for indicator.");
        return -1;
    }
    return 0;

}
void indicator_remove_expression_file(struct indicator_kmalloc_state* state)
{
    mutex_lock(&indicator_mutex);
    state->expression_file->d_inode->i_private = NULL;
    mutex_unlock(&indicator_mutex);

    debugfs_remove(state->expression_file);
}
/////Implementation of getter and setter for file operations/////////////
char* indicator_expression_file_get_str(struct inode* inode)
{
    char *str;
    struct indicator_kmalloc_state* state;
   
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        return NULL;
    }

    state = inode->i_private;
    if(state)
    {
        str = kstrdup(state->expression, GFP_KERNEL);
    }
    else
    {
        str = NULL;//'device', corresponed to file, is not exist
    }
    mutex_unlock(&indicator_mutex);
    
    return str;
}
int indicator_expression_file_set_str(const char* str, struct inode* inode)
{
    int error;
    struct indicator_kmalloc_state* state;
    
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        return -EINTR;
    }

    state = inode->i_private;
    if(state)
    {
        error = indicator_state_expression_set_internal(state, str);
    }
    else
    {
        error = -EINVAL;//'device', corresponed to file, is not exist
    }
    mutex_unlock(&indicator_mutex);
    return error;
}
