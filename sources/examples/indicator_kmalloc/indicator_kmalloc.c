#include <linux/module.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() (for our needs) */
//previous header already include this
//#include <linux/gfp.h>      /* gfp_flags constants*/

#include <linux/debugfs.h>

#include <linux/uaccess.h> /* copy_*_user functions */

#include <linux/mutex.h>

#include <kedr/fault_simulation/fault_simulation.h>
#include "calculator/calculator.h"


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
    char* expression;
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
    
    kedr_calc_int_t vars[2];
    vars[0] = (kedr_calc_int_t)point_data->size;
    vars[1] = (kedr_calc_int_t)point_data->flags;
    
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
        ARRAY_SIZE(var_names), var_names);
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
        ARRAY_SIZE(var_names), var_names);
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
// Helper functions, which used in implementation of file operations

//Helper for read operation of file. 'As if' it reads file, contatining string.
static ssize_t string_operation_read(const char* str, char __user *buf, size_t count, 
	loff_t *f_pos);

//Helper for llseek operation of file. Help to user space utilities to find out size of file.
static loff_t string_operation_llseek (const char* str, loff_t *f_pos, loff_t offset, int whence);

// Helper function for write operation of file. Allocate buffer, which contain writting string.
// On success(non-negative value is returned), out_str should be freed when no longer needed.
static ssize_t
string_operation_write(char** out_str, const char __user *buf,
    size_t count, loff_t *f_pos);


/////////////////////real operations///////////////
static loff_t
indicator_expression_file_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t result;
    
    struct indicator_kmalloc_state* state;
   
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        return -EINTR;
    }

    state = filp->f_dentry->d_inode->i_private;
    if(state)
    {
        result = string_operation_llseek(state->expression, &filp->f_pos, off, whence);
    }
    else
    {
        result = -EINVAL;//'device', corresponed to file, is not exist
    }
    mutex_unlock(&indicator_mutex);
    
    return result;
}

static ssize_t 
indicator_expression_file_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
    ssize_t result;

    struct indicator_kmalloc_state* state;
   
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        return -EINTR;
    }

    state = filp->f_dentry->d_inode->i_private;
    if(state)
    {
        result = string_operation_read(state->expression, buf, count, f_pos);
    }
    else
    {
        result = -EINVAL;//'device', corresponed to file, is not exist
    }
    mutex_unlock(&indicator_mutex);
    
    return result;
}

static ssize_t 
indicator_expression_file_write(struct file *filp, const char __user *buf,
    size_t count, loff_t *f_pos)
{
    ssize_t result;
    char* write_str;

    struct indicator_kmalloc_state* state;
    
    result = string_operation_write(&write_str, buf, count, f_pos);
    
    if(result < 0) return result;
   
    if(mutex_lock_killable(&indicator_mutex))
    {
        debug0("Operation was killed");
        kfree(write_str);
        return -EINTR;
    }

    state = filp->f_dentry->d_inode->i_private;
    if(state)
    {
        result = indicator_state_expression_set_internal(state, write_str);
    }
    else
    {
        result = -EINVAL;//'device', corresponed to file, is not exist
    }
    mutex_unlock(&indicator_mutex);
    kfree(write_str);    
    return result ? result : count;

}

struct file_operations indicator_expression_file_operations =
{
    .owner = THIS_MODULE,//
    .llseek = indicator_expression_file_llseek,//
    .read = indicator_expression_file_read,//
    .write = indicator_expression_file_write
};
//
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
//
//Helper for read operation of file. 'As if' it reads file, contatining string.
ssize_t string_operation_read(const char* str, char __user *buf, size_t count, 
	loff_t *f_pos)
{
    //length of 'file'(include terminating '\0')
    size_t size = strlen(str) + 1;
    //whether position out of range
    if((*f_pos < 0) || (*f_pos > size)) return -EINVAL;
    if(*f_pos == size) return 0;// eof

    if(count + *f_pos > size)
        count = size - *f_pos;
    if(copy_to_user(buf, str + *f_pos, count) != 0)
        return -EFAULT;
    
    *f_pos += count;
    return count;
}

//Helper for llseek operation of file. Help to user space utilities to find out size of file.
loff_t string_operation_llseek (const char* str, loff_t *f_pos, loff_t offset, int whence)
{
    loff_t new_offset;
    size_t size = strlen(str) + 1;
    switch(whence)
    {
    case 0: /* SEEK_SET */
        new_offset = offset;
    break;
    case 1: /* SEEK_CUR */
        new_offset = *f_pos + offset;
    break;
    case 2: /* SEEK_END */
        new_offset = size + offset;
    break;
    default: /* can't happen */
        return -EINVAL;
    };
    if(new_offset < 0) return -EINVAL;
    if(new_offset > size) new_offset = size;//eof
    
    *f_pos = new_offset;
    //returning value is offset from the beginning, filp->f_pos, generally speaking, may be any.
    return new_offset;
}

ssize_t
string_operation_write(char** out_str, const char __user *buf,
    size_t count, loff_t *f_pos)
{
    char* buffer;

    if(count == 0)
    {
        pr_err("write: 'count' shouldn't be 0.");
        return -EINVAL;
    }

    /*
     * Feature of control files.
     *
     * Because writting to such files is really command to the module to do something,
     * and successive reading from this file return total effect of this command.
     * it is meaningless to process writting not from the start.
     *
     * In other words, writting always affect to the global content of the file.
     */
    if(*f_pos != 0)
    {
        pr_err("Partial rewritting is not allowed.");
        return -EINVAL;
    }
    //Allocate buffer for writting value - for its preprocessing.
    buffer = kmalloc(count + 1, GFP_KERNEL);
    if(buffer == NULL)
    {
        pr_err("Cannot allocate buffer.");
        return -ENOMEM;
    }

    if(copy_from_user(buffer, buf, count) != 0)
    {
        pr_err("copy_from_user return error.");
        kfree(buffer);
        return -EFAULT;
    }
    // For case, when one try to write not null-terminated sequence of bytes,
    // or omit terminated null-character.
    buffer[count] = '\0';

    /*
     * Usually, writting to the control file is performed via 'echo' command,
     * which append new-line symbol to the writting string.
     *
     * Because, this symbol is usually not needed, we trim it.
     */
    if(buffer[count - 1] == '\n') buffer[count - 1] = '\0';

    *out_str = buffer;
    return count;
}