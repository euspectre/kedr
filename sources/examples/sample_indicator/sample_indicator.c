/*
 * Example of using fault simulation API to create fault simulation indicator module.
 */

/* ========================================================================
 * Copyright (C) 2010, Institute for System Programming 
 *                     of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>

#include <kedr/fault_simulation/fault_simulation.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/debugfs.h> /* for create files, describing indicator*/
#include <linux/uaccess.h> /* copy_to_user */

#include <linux/mutex.h>


MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#define DEFAULT_PERIOD 2

/*
 * Name of the indicator (it is not the name of the module!),
 * using which one may set indicator for the particular fault simulation point.
 */

const char* sample_indicator_name = "periodic";

/*
 * This indicator doesn't accepts parameters from the point.
 * Otherwise we should declare structure like next, which describe parameters:
 *
 *
 * struct sample_indicator_parameters
 * {
 *     //parameters, which indicator accepts in the simulate call.
 *     //e.g
 *     size_t size;
 *     void* addr;
 * };
 *
 */

/*
 * String which should describe parameters needed by the indicator at simulate call.
 *
 * It is used to prevent setting indicator for the point, which is not supply all needed parameters.
 *
 * This string should contains comma-separated types of parameters in the same order, as them
 * was described in the indicator_parameters' structure.
 *
 * E.g., for the parameters
 *
 * struct indicator_parameters
 * {
 *      size_t size;
 *      void* addr;
 * };
 *
 * format string should be "size_t,void*".
 *
 * Because our indicator doesn't need any parameter, its format string is empty.
 */

const char* sample_indicator_format_string = "";

/*
 * Describe parameters, which may be unique for every instance of indicator.
 *
 * These parameters are packed into structure, which is passed to the indicator at simulate stage
 * and to the other callbacks.
 */

struct sample_indicator_state
{
    /*
     * Period of simulating failes.
     *
     * This parameter is passed to the indicator in init function for indicator instance as string.
     * After this, parameter become readonly.
     */
    unsigned long period;
    
    /*
     * Count of the call simulate() function.
     *
     * When indicator is set for the point, this count sets to 0.
     * It is incremented every call of simulate function via atomic_inc.
     */
    atomic_t times;
    
    /*
     * File which contains period of the given indicator instance.
     */
    struct dentry* file_period;
};

/*
 * Protect state of instance indicator instance from concurrent access via reading from the file and deleting this instance.
 *
 * The thing is that it is possible to read from the file descriptor
 * even after this file will be deleting using debugfs_remove().
 */

DEFINE_MUTEX(indicator_mutex);

//////////////Indicator's functions declaration////////////////////////////

/*
 * Indicator's initialize function.
 *
 * It is called whenever indicator should be set for the fault simulation point.
 *
 * It accept string, which was passed to the indicator and may be interpreted
 * by the indicator in its own way.
 *
 * ( How to pass this string:
 *
 * echo "<indicator_name> <params>" > .../<point_name>/current_indicator
 *
 * ).
 *
 * Also it accepts point's control directory and may create some files in this directory.
 *
 * Function should return 0 on success and not 0 if it cannot initialize indicator.
 *
 * On success, pointer to the indicator instance's state should be stored in the
 * 'indicator_state' variable.
 */

static int sample_indicator_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory);

/*
 * Indicator's simulate function.
 *
 * It accept pointer to the state of the current indicator instance,
 * and the pointer to the parameters structure, which is passed from the 
 * fault simulation point.
 *
 * Function should return result of simulation:
 *  not 0 value tells to the fault simulation point to simulate failure.
 *
 * Also it may update state.
 */
static int sample_indicator_simulate(void* indicator_state, void* user_data);

/*
 * Indicator's destroy function.
 *
 * It accept pointer to the state of the current indicator instance.
 *
 * Function should release any resources, which is requested at initialization,
 * and remove all files, which was created in the point's control directory.
 */

static void sample_indicator_instance_destroy(void* indicator_state);

/* 
 * Reference to the indicator which will be registered.
 */
struct kedr_simulation_indicator* sample_indicator;

static int __init
sample_indicator_init(void)
{
    // Registering of the indicator.
    sample_indicator = kedr_fsim_indicator_register(
        sample_indicator_name,
        sample_indicator_simulate,
        sample_indicator_format_string, 
        sample_indicator_instance_init,
        sample_indicator_instance_destroy);

    if(sample_indicator == NULL)
    {
        pr_err("Cannot register indicator '%s'.\n", sample_indicator_name);
        return -EINVAL;
    }

    return 0;
}

static void
sample_indicator_exit(void)
{
	// Deregistration of the indicator
	kedr_fsim_indicator_unregister(sample_indicator);
	return;
}

module_init(sample_indicator_init);
module_exit(sample_indicator_exit);

////////////Implementation of indicator's functions////////////////////

// Declare common file operation structure, which will be used for
// all file_period instances.

static ssize_t file_period_read(struct file *filp,
    char __user *buf, size_t count, loff_t *f_pos);

struct file_operations file_period_operations =
{    
    .owner = THIS_MODULE,
    .open = nonseekable_open,
    .read = file_period_read
};


int sample_indicator_instance_init(void** indicator_state,
    const char* params, struct dentry* control_directory)
{
    struct sample_indicator_state* sample_indicator_state;
    unsigned long period;
    // Read 'params' and convert its to the value of the period
    if((params != NULL) && (*params != '\0'))
    {
        //Without parameters, set period to the default value
        period = DEFAULT_PERIOD;
    }
    else
    {
        int error = strict_strtoul(params, 10, &period);
        if(error)
        {
            pr_err("Cannot convert '%s' to unsigned long period.", params);
            return error;
        }
        if(period <= 0)
        {
            pr_err("Period should be positive.");
            return -EINVAL;
        }
    }
    // Allocate state instance.
    sample_indicator_state = kmalloc(sizeof(*sample_indicator_state), GFP_KERNEL);
    if(sample_indicator_state == NULL)
    {
        pr_err("Cannot allocate indicator state instance.");
        return -ENOMEM;
    }
    //Set state variables
    sample_indicator_state->period = period;
    atomic_set(&sample_indicator_state->times, 0);
    // Create file in the fault simulation point's directory
    sample_indicator_state->file_period = debugfs_create_file("period",
        S_IRUGO,
        control_directory,
        sample_indicator_state,
        &file_period_operations);
    
    if(sample_indicator_state->file_period == NULL)
    {
        pr_err("Cannot create file for period.");
        kfree(sample_indicator_state);
        return -EINVAL;
    }
    //Store indicator state
    *indicator_state = sample_indicator_state;
    return 0;
}

int sample_indicator_simulate(void* indicator_state, void* user_data)
{
    unsigned long times;
    /*
     * 'indicator_state' is really what we has stored in the init function,
     * that is 'struct sample_indicator_state*'.
     */

    struct sample_indicator_state* sample_indicator_state = 
        (struct sample_indicator_state*) indicator_state;
    
    /*
     * Because our indicator doesn't take parameters from the 
     * fault simulation point, second argument of this function
     * shoudn't be used.
     *
     * Otherwise it may be casted to the 'struct indicator_parameters'.
     */
    (void)user_data;
    // Update 'times' counter in the state
    times = atomic_inc_return(&sample_indicator_state->times);
    // Return not 0 if current 'times' is multiple of 'period'.
    return (times % sample_indicator_state->period) == 0;
}

void sample_indicator_instance_destroy(void* indicator_state)
{
    /*
     * 'indicator_state' is really what we has stored in the init function,
     * that is 'struct sample_indicator_state*'.
     */

    struct sample_indicator_state* sample_indicator_state = 
        (struct sample_indicator_state*) indicator_state;

    // Firstly, remove file
    // Clear reference to the indicator state under mutex locked.
    mutex_lock(&indicator_mutex);
    sample_indicator_state->file_period->d_inode->i_private = NULL;
    mutex_unlock(&indicator_mutex);
    
    debugfs_remove(sample_indicator_state->file_period);

    // Finally, free state
    kfree(sample_indicator_state);
}

//////////////////Implementation file operations////////////////
ssize_t file_period_read(struct file *filp,
    char __user *buf, size_t count, loff_t *f_pos)
{
    char str[20];
    size_t size;
    struct sample_indicator_state* sample_indicator_state;

    //Read field with private data in inode under mutex taken.
    mutex_lock(&indicator_mutex);
    sample_indicator_state = filp->f_dentry->d_inode->i_private;
    mutex_unlock(&indicator_mutex);
    
    if(sample_indicator_state == NULL) return -EINVAL;//file already removed
    
    size = scnprintf(str, sizeof(str), "%lu\n", sample_indicator_state->period) + 1;

    //Standard writting string to the buf in read file operation
    if((*f_pos < 0) || (*f_pos > size)) return -EINVAL;
    if(*f_pos == size) return 0;// eof

    if(count + *f_pos > size)
        count = size - *f_pos;

    if(copy_to_user(buf, str + *f_pos, count) != 0)
        return -EFAULT;

    *f_pos += count;
    return count;
}
