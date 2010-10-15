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
#include <kedr/fault_simulation/calculator.h>
#include <kedr/control_file/control_file.h>

#include <kedr/base/common.h> /* in_init */
#include <linux/sched.h> /* task_pid */
#include <linux/random.h> /* random32() */

// Macros for unify output information to the kernel log file
#define debug(str, ...) pr_debug("%s: " str, __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) pr_err("%s: " str, __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

<$global$>

// Constants in the expression
<$expressionConstDeclarations$>

// Variables in the expression
static const char* var_names[]= {
    "times",//local variable of indicator state
<$expressionVarNames$>
};
// Runtime variables in the expression
static kedr_calc_int_t in_init_weak_var_compute(void)
{
    return kedr_target_module_in_init();
}

static kedr_calc_int_t rnd100_weak_var_compute(void)
{
    return random32() % 100;
}

static kedr_calc_int_t rnd10000_weak_var_compute(void)
{
    return random32() % 10000;
}

<$expressionRvarFunctions$>

static const struct kedr_calc_weak_var weak_vars[] = {
    { .name = "in_init", .compute = in_init_weak_var_compute },
    { .name = "rnd100", .compute = rnd100_weak_var_compute },
    { .name = "rnd10000", .compute = rnd10000_weak_var_compute },
<$expressionRvarDeclarations$>
};

// Indicator parameters
<$pointDataType$>

struct indicator_real_state
{
    kedr_calc_t* calc;
    char* expression;
    atomic_t times;
    //if not 0, we shouldn't make fail processes which is not derived from process of this pid.
    //should have java volatile-like behaviour
    atomic_t pid;
    //
    struct dentry* expression_file;
    struct dentry* pid_file;
};
//Protect from concurrent access all except read from 'struct indicator_real_state'.'calc'.
// That read is protected by rcu.
DEFINE_MUTEX(indicator_mutex);

////////////////Auxiliary functions///////////////////////////

// Initialize expression part of the indicator state.
static int
indicator_state_expression_init(struct indicator_real_state* state, const char* expression);
// Change expression of the indicator. On fail state is not changed.
// Should be executed with mutex taken.
static int
indicator_state_expression_set_internal(struct indicator_real_state* state, const char* expression);
//Determine according to the value of 'pid' field of the indicator state,
//whether we may make fail this process or not.
static int process_may_fail(pid_t filter_pid);
// Change pid-constraint of the indicator.
// Should be executed with mutex taken.
static int
indicator_state_pid_set_internal(struct indicator_real_state* state, pid_t pid);

// Destroy expression part of the indicator state
static void
indicator_state_expression_destroy(struct indicator_real_state* state);
//
static int indicator_create_expression_file(struct indicator_real_state* state, struct dentry* dir);
static void indicator_remove_expression_file(struct indicator_real_state* state);
//
static int indicator_create_pid_file(struct indicator_real_state* state, struct dentry* dir);
static void indicator_remove_pid_file(struct indicator_real_state* state);

//////////////Indicator's functions declaration////////////////////////////

static int indicator_simulate(void* indicator_state, void* user_data);
static void indicator_instance_destroy(void* indicator_state);
static int indicator_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory);

struct kedr_simulation_indicator* indicator;

static int __init
indicator_init(void)
{
    indicator = kedr_fsim_indicator_register("<$indicator.name$>",
        indicator_simulate, "<$indicatorFormatString$>",
        indicator_instance_init,
        indicator_instance_destroy);
    if(indicator == NULL)
    {
        printk(KERN_ERR "Cannot register indicator.\n");
        return 1;
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

////////////Implementation of indicator's functions////////////////////

int indicator_simulate(void* indicator_state, void* user_data)
{
    int result = 0;

    struct indicator_real_state* indicator_real_state =
        (struct indicator_real_state*)indicator_state;
    
    smp_rmb();//volatile semantic of 'pid' field
    if(process_may_fail((pid_t)atomic_read(&indicator_real_state->pid)))
    {

        kedr_calc_int_t vars[ARRAY_SIZE(var_names)];

        vars[0] = atomic_inc_return(&indicator_real_state->times);
<$pointDataUse$>

<$expressionVarsSet$>

<$pointDataUnuse$>

        rcu_read_lock();
        result = kedr_calc_evaluate(rcu_dereference(indicator_real_state->calc), vars);
        rcu_read_unlock();
    }
    return result;
}

void indicator_instance_destroy(void* indicator_state)
{
    struct indicator_real_state* indicator_real_state =
        (struct indicator_real_state*)indicator_state;
    
    indicator_remove_pid_file(indicator_real_state);
    indicator_remove_expression_file(indicator_real_state);
    indicator_state_expression_destroy(indicator_real_state);
    
    kfree(indicator_real_state);
}

int indicator_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory)
{
    struct indicator_real_state* indicator_real_state;
    const char* expression = (*params != '\0') ? params : "0";
    indicator_real_state = kmalloc(sizeof(*indicator_real_state), GFP_KERNEL);
    if(indicator_real_state == NULL)
    {
        pr_err("Cannot allocate indicator state.");
        return -1;
    }
    if(indicator_state_expression_init(indicator_real_state, expression))
    {
        kfree(indicator_real_state);
        return -1;
    }
    
    atomic_set(&indicator_real_state->pid, 0);//initially - no process filtering
    
    if(indicator_create_pid_file(indicator_real_state, control_directory))
    {
        indicator_state_expression_destroy(indicator_real_state);
        kfree(indicator_real_state);
        return -1;
    }
    
    if(indicator_create_expression_file(indicator_real_state, control_directory))
    {
        indicator_remove_pid_file(indicator_real_state);
        indicator_state_expression_destroy(indicator_real_state);
        kfree(indicator_real_state);
        return -1;
    }
    
    *indicator_state = indicator_real_state;
    return 0;
}
//////////////////Implementation of auxiliary functions////////////////
// Initialize expression part of the indicator state.
static int
indicator_state_expression_init(struct indicator_real_state* state, const char* expression)
{
    state->calc = kedr_calc_parse(expression,
        <$expressionConstParams$>,
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
// Change expression of the indicator. On fail state is not changed.
// Should be executed with mutex taken.
static int
indicator_state_expression_set_internal(struct indicator_real_state* state, const char* expression)
{
    char *new_expression;
    kedr_calc_t *old_calc, *new_calc;
    
    new_calc = kedr_calc_parse(expression,
        <$expressionConstParams$>,
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
indicator_state_expression_destroy(struct indicator_real_state* state)
{
    kedr_calc_delete(state->calc);
    kfree(state->expression);
}

//Determine according to the value of 'pid' field of the indicator state,
//whether we may make fail this process or not.
int process_may_fail(pid_t filter_pid)
{
    struct task_struct* t, *t_prev;
    int result = 0;
    if(filter_pid == 0) return 1;
    
    //read list in rcu-protected manner(perhaps, rcu may sence)
    rcu_read_lock();
    for(t = current, t_prev = NULL; (t != NULL) && (t != t_prev); t_prev = t, t = rcu_dereference(t->parent))
    {
        if(task_tgid_vnr(t) == filter_pid) 
        {
            result = 1;
            break;
        }
    }
    rcu_read_unlock();
    return result;
}
// Change pid-constraint of the indicator.
// Should be executed with mutex taken.
int
indicator_state_pid_set_internal(struct indicator_real_state* state, pid_t pid)
{
    atomic_set(&state->pid, pid);
    //write under write lock, so doesn't need barriers
    return 0;
}


/////////////////////Files implementation/////////////////////////////
//Expression
static char* indicator_expression_file_get_str(struct inode* inode);
static int indicator_expression_file_set_str(const char* str, struct inode* inode);

CONTROL_FILE_OPS(indicator_expression_file_operations,
    indicator_expression_file_get_str, indicator_expression_file_set_str);

int indicator_create_expression_file(struct indicator_real_state* state, struct dentry* dir)
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
void indicator_remove_expression_file(struct indicator_real_state* state)
{
    mutex_lock(&indicator_mutex);
    state->expression_file->d_inode->i_private = NULL;
    mutex_unlock(&indicator_mutex);

    debugfs_remove(state->expression_file);
}
// Pid
static char* indicator_pid_file_get_str(struct inode* inode);
static int indicator_pid_file_set_str(const char* str, struct inode* inode);

CONTROL_FILE_OPS(indicator_pid_file_operations,
    indicator_pid_file_get_str, indicator_pid_file_set_str);

int indicator_create_pid_file(struct indicator_real_state* state, struct dentry* dir)
{
    state->pid_file = debugfs_create_file("pid",
        S_IRUGO | S_IWUSR | S_IWGRP,
        dir,
        state, &indicator_pid_file_operations);
    if(state->pid_file == NULL)
    {
        pr_err("Cannot create pid file for indicator.");
        return -1;
    }
    return 0;

}
void indicator_remove_pid_file(struct indicator_real_state* state)
{
    mutex_lock(&indicator_mutex);
    state->pid_file->d_inode->i_private = NULL;
    mutex_unlock(&indicator_mutex);

    debugfs_remove(state->pid_file);
}
//
/////Implementation of getter and setter for file operations/////////////
char* indicator_expression_file_get_str(struct inode* inode)
{
    char *str;
    struct indicator_real_state* state;
   
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
    struct indicator_real_state* state;
    
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

static char* pid_to_str(pid_t pid)
{
    char *str;
    int str_len;

    //write pid as 'long'
    str_len = snprintf(NULL, 0, "%ld", (long)pid);
    
    str = kmalloc(str_len + 1, GFP_KERNEL);
    if(str == NULL)
    {
        pr_err("Cannot allocate string for pid");
        return NULL;
    }
    snprintf(str, str_len + 1, "%ld", (long)pid);
    return str;
}

int str_to_pid(const char* str, pid_t* pid)
{
    //read pid as long
    long pid_long;
    int result = strict_strtol(str, 10, &pid_long);
    if(!result)
        *pid = (pid_t)pid_long;
    return result;
}

char* indicator_pid_file_get_str(struct inode* inode)
{
    struct indicator_real_state* state;
    pid_t pid;
   
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        return NULL;
    }
    state = inode->i_private;
    if(state)
        pid = atomic_read(&state->pid);//read under write lock, so doesn't need barriers

    mutex_unlock(&indicator_mutex);
    
    if(!state) return NULL;//'device', corresponed to file, doesn't not exist
    
    return pid_to_str(pid);
}
int indicator_pid_file_set_str(const char* str, struct inode* inode)
{
    int error;
    struct indicator_real_state* state;
    pid_t pid;
    
    error = str_to_pid(str, &pid);
    if(error) return -EINVAL;
        
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        return -EINTR;
    }

    state = inode->i_private;
    if(state)
    {
        error = indicator_state_pid_set_internal(state, pid);
    }
    else
    {
        error = -EINVAL;//'device', corresponed to file, is not exist
    }
    mutex_unlock(&indicator_mutex);
    return error;
}
