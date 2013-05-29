/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Copyright (C) 2010-2012, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <kedr/fault_simulation/fault_simulation.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/list.h>		/* list functions */

#include <linux/module.h>
	
#include <linux/mutex.h>

#include <linux/uaccess.h> /* copy_*_user functions */
	
#include <linux/ctype.h> /* isspace() */

#include <linux/string.h> /* memcpy */

#include <kedr/control_file/control_file.h>
	
MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

struct indicator_instance;

/*
 * Structure described simulation point
 */

struct kedr_simulation_point
{
	// Current indicator instance set for the point. NULL if not set.
	struct indicator_instance* current_instance;
	// List organization for point.
	struct list_head list;
	const char* name;
	const char* format_string;
	// Control directory for the point
	struct dentry* control_dir;
	// .. and files in it
	struct dentry* format_string_file;
	struct dentry* indicator_file;
};

struct kedr_simulation_indicator
{
	// List organization of indicators
	struct list_head list;
	// Indicators's data
	const char* name;
	const char* format_string;
	// Callbacks
	int (*simulate)(void* indicator_state, void* user_data);
	int (*create_instance)(void** indicator_state,
		const char* params, struct dentry* control_directory);
	void (*destroy_instance)(void* indicator_state);
	// List of instances for this indicator
	struct list_head instances;
	//control directory for indicator
	struct dentry* control_dir;
};

struct indicator_instance
{
	// List organization of instances for the same indicator
	struct list_head list;
	// Indicator to which instance belongs
	struct kedr_simulation_indicator* indicator;
	// State of the instance(created and used by indicator's callbacks)
	void* indicator_state;
	// Point for which this instance is set
	struct kedr_simulation_point* current_point;
};
//This indicator name corresponds to the case, when indicator doesn't set for the point.
// It only used in control files for read/write operations.
static const char* indicator_name_not_set = "none";

//List of points
static LIST_HEAD(points);
//List of indicators
static LIST_HEAD(indicators);
/*
 *  Mutex protecting from concurrent access:
 * 
 * -list of points
 * -list of indicators
 * -indicator instance for the point(only writes, r/w is protected by rcu)
 */
static DEFINE_MUTEX(fsim_mutex);
// Top-level directories for fault simulation
static struct dentry* root_directory;
static struct dentry* points_root_directory;
static struct dentry* indicators_root_directory;

// File for access last fault.
static struct dentry* last_fault_file;

static char kedr_fsim_fault_message_buf[KEDR_FSIM_FAULT_MESSAGE_LEN + 1] = "none";
static DEFINE_SPINLOCK(kedr_fsim_fault_message_lock);

// Auxiliary functions

/*
 * Return simulation point with given name or NULL.
 *
 * Should be executed with mutex taken.
 */

static struct kedr_simulation_point* lookup_point(const char* name);

/*
 * Return simulation indicator with given name or NULL.
 *
 * Should be executed with mutex taken.
 */

static struct kedr_simulation_indicator* lookup_indicator(const char* name);

/*
 * Destroy instance of indicator.
 * 
 * Instance should be already unset for the point.
 */
static void
indicator_instance_destroy(struct indicator_instance* instance);

/*
 *  Verify, whether data, which format is described in
 *  'point_format_string', will be correctly interpreted by indicator,
 *  which expect data in 'indicator_format_string' format.
 * 
 *  Return not 0 on success, 0 otherwise.
 */
static int
is_data_format_compatible(	const char* point_format_string,
							const char* indicator_format_string);

/*
 * Create and set indicator instance for the point.
 * 
 * For reuse function for exported kedr_fsim_point_set_indicator() and for
 * changing indicator via writing file
 *
 * Should be executed with mutex locked.
 */
static int kedr_fsim_point_set_indicator_internal(
	struct kedr_simulation_point* point,
	const char* indicator_name,
	const char* params);

/*
 * Clear indicator instance for the point, if needed.
 *
 * Should be executed with mutex locked.
 */
static void kedr_fsim_point_clear_indicator_internal(
	struct kedr_simulation_point* point);

/*
 * Create directories and files for new point.
 *
 * Should be executed with mutex locked.
 */
static int create_point_files(struct kedr_simulation_point* point);
static void delete_point_files(struct kedr_simulation_point* point);

/*
 * Create directory for indicator
 *
 * Should be executed with mutex locked.
 */
static int create_indicator_files(struct kedr_simulation_indicator* indicator);
static void delete_indicator_files(struct kedr_simulation_indicator* indicator);

//////////////////Implementation of exported functions////////////////////

struct kedr_simulation_point* 
kedr_fsim_point_register(const char* point_name,
	const char* format_string)
{
	struct kedr_simulation_point* point = NULL;

	if(mutex_lock_killable(&fsim_mutex))
	{
		return NULL;
	}
	
	if(lookup_point(point_name))
	{
		print_error("Point with name '%s' is already registered.", point_name);
		goto out;
	}
	point = kmalloc(sizeof(*point), GFP_KERNEL);
	if(point == NULL)
	{
		print_error0("Cannot allocate memory for the fault simulation point.");
		goto out;
	}
	point->name = point_name;
	point->format_string = format_string ? format_string : "";
	point->current_instance = NULL;
	
	if(create_point_files(point))
	{
		kfree(point);
		point = NULL;
		goto out;
	}

	list_add(&point->list, &points);

	mutex_unlock(&fsim_mutex);

out:    
	return point;
}
EXPORT_SYMBOL(kedr_fsim_point_register);

void kedr_fsim_point_unregister(struct kedr_simulation_point* point)
{
	if(mutex_lock_killable(&fsim_mutex))
	{
		return;
	}

	kedr_fsim_point_clear_indicator_internal(point);

	list_del(&point->list);
	delete_point_files(point);
	kfree(point);

	mutex_unlock(&fsim_mutex);
}
EXPORT_SYMBOL(kedr_fsim_point_unregister);

struct kedr_simulation_indicator* 
kedr_fsim_indicator_register(const char* indicator_name,
	int (*simulate)(void* indicator_state, void* user_data),
	const char* format_string,
	int (*create_instance)(void** indicator_state,
		const char* params, struct dentry* control_directory),
	void (*destroy_instance)(void* indicator_state))
{
	struct kedr_simulation_indicator *indicator = NULL;
	if(mutex_lock_killable(&fsim_mutex))
	{
		return NULL;
	}

	if(lookup_indicator(indicator_name))
	{
		print_error("Indicator with name '%s' is already registered.", indicator_name);
		goto out;
	}
	
	indicator = kmalloc(sizeof(*indicator), GFP_KERNEL);
	if(indicator == NULL)
	{
		print_error0("Cannot allocate memory for indicator.");
		goto out;
	}
	indicator->name = indicator_name;
	indicator->format_string = format_string;
	indicator->simulate = simulate;
	indicator->create_instance = create_instance;
	indicator->destroy_instance = destroy_instance;
	INIT_LIST_HEAD(&indicator->instances);
	INIT_LIST_HEAD(&indicator->list);
	
	if(create_indicator_files(indicator))
	{
		kfree(indicator);
		indicator = NULL;
		goto out;
	}

	list_add(&indicator->list, &indicators);

out:
	mutex_unlock(&fsim_mutex);
	
	return indicator;
}
EXPORT_SYMBOL(kedr_fsim_indicator_register);

void kedr_fsim_indicator_unregister(struct kedr_simulation_indicator* indicator)
{
	if(mutex_lock_killable(&fsim_mutex))
	{
		return;
	}

	if(!list_empty(&indicator->instances))
	{
		struct indicator_instance* instance;
		list_for_each_entry(instance, &indicator->instances, list)
		{
			rcu_assign_pointer(instance->current_point->current_instance, NULL);
		}

		synchronize_rcu();
		
		while(!list_empty(&indicator->instances))
		{
			instance = list_first_entry(&indicator->instances,
				struct indicator_instance, list);
			indicator_instance_destroy(instance);
		}
	}

	list_del(&indicator->list);
	delete_indicator_files(indicator);
	kfree(indicator);

	mutex_unlock(&fsim_mutex);
}
EXPORT_SYMBOL(kedr_fsim_indicator_unregister);


int kedr_fsim_point_set_indicator(const char* point_name,
	const char* indicator_name, const char* params)
{
	struct kedr_simulation_point* point;
	int result;
	if(mutex_lock_killable(&fsim_mutex))
	{
		return -EAGAIN;
	}
	
	point = lookup_point(point_name);
	if(point == NULL)
	{
		print_error("Point with name '%s' does not exist.", point_name);
		result = -ENOENT;
		goto out;
	}

	result = kedr_fsim_point_set_indicator_internal(point, indicator_name, params);

	mutex_unlock(&fsim_mutex);
out:
	return result;
}
EXPORT_SYMBOL(kedr_fsim_point_set_indicator);


int kedr_fsim_point_clear_indicator(const char* point_name)
{
	struct kedr_simulation_point* point;

	int result;
	
	if(mutex_lock_killable(&fsim_mutex))
	{
		return -EAGAIN;
	}
	
	point = lookup_point(point_name);
	if(point == NULL)
	{
		print_error("Point with name '%s' does not exist.", point_name);
		result = -ENOENT;
		goto err;
	}

	kedr_fsim_point_clear_indicator_internal(point);

	mutex_unlock(&fsim_mutex);
	return 0;

err:
	mutex_unlock(&fsim_mutex);
	return result;
}
EXPORT_SYMBOL(kedr_fsim_point_clear_indicator);

int kedr_fsim_point_simulate(struct kedr_simulation_point* point,
	void *user_data)
{
	int result;
	struct indicator_instance* current_instance;

	rcu_read_lock();
	
	current_instance = rcu_dereference(point->current_instance);
	
	result = current_instance
		? current_instance->indicator->simulate(
			current_instance->indicator_state, user_data)
		: 0;

	rcu_read_unlock();

	return result;
}
EXPORT_SYMBOL(kedr_fsim_point_simulate);

int kedr_fsim_fault_message(const char* fmt, ...)
{
	int len;
	unsigned long flags;
	va_list args;
	
	spin_lock_irqsave(&kedr_fsim_fault_message_lock, flags);
	
	va_start(args, fmt);
	len = vsnprintf(kedr_fsim_fault_message_buf, KEDR_FSIM_FAULT_MESSAGE_LEN + 1, fmt, args);
	va_end(args);
	
	spin_unlock_irqrestore(&kedr_fsim_fault_message_lock, flags);
	
	return len > KEDR_FSIM_FAULT_MESSAGE_LEN;
}
EXPORT_SYMBOL(kedr_fsim_fault_message);
///////////////////Implementation of auxiliary functions/////////////////////

/*
 * Return simulation point with given name or NULL.
 *
 * Should be executed under mutex taken.
 */

static struct kedr_simulation_point *
lookup_point(const char* name)
{
	struct kedr_simulation_point* point;
	list_for_each_entry(point, &points, list)
	{
		if(strcmp(point->name, name) == 0) return point;
	}
	return NULL;
}

/*
 * Same for indicators
 */

static struct kedr_simulation_indicator *
lookup_indicator(const char* name)
{
	struct kedr_simulation_indicator* indicator;
	list_for_each_entry(indicator, &indicators, list)
	{
		if(strcmp(indicator->name, name) == 0) return indicator;
	}
	return NULL;


}

static void 
indicator_instance_destroy(struct indicator_instance* instance)
{
	struct kedr_simulation_indicator* indicator = instance->indicator;

	list_del(&instance->list);
	
	if(indicator->destroy_instance)
		indicator->destroy_instance(instance->indicator_state);

	kfree(instance);
}

static int
is_data_format_compatible(const char* point_format_string,
	const char* indicator_format_string)
{
	if(indicator_format_string == NULL
		|| *indicator_format_string == '\0')
	{
		//always compatible
		return 1;
	}
	else if(point_format_string == NULL
		|| *point_format_string == '\0')
	{
		//no data are passed, but indicator expects something
		return 0;
	}
	// simple verification, may be changed in the future
	return strncmp(point_format_string, indicator_format_string,
		strlen(indicator_format_string)) == 0;
}

static int 
kedr_fsim_point_set_indicator_internal(struct kedr_simulation_point* point,
	const char* indicator_name, const char* params)
{
	struct indicator_instance *instance;
	struct kedr_simulation_indicator* indicator;
	
	indicator = lookup_indicator(indicator_name);
	if(indicator == NULL)
	{
		print_error("Indicator with name '%s' does not exist.", indicator_name);
		return -ENODEV;
	}

	if(!is_data_format_compatible(point->format_string, indicator->format_string))
	{
		print_error("Indicator with name '%s' has format of parameters '%s', "
			"which is not compatible with format '%s' used by the point with name '%s'.",
			indicator_name, indicator->format_string,
			point->format_string, point->name);
		return -EINVAL;
	}
	
	instance = kmalloc(sizeof(*instance), GFP_KERNEL);
	if(instance == NULL)
	{
		print_error0("Cannot allocate memory for instance of indicator.");
		return -ENOMEM;
	}

	kedr_fsim_point_clear_indicator_internal(point);

	instance->indicator_state = NULL;
	if(indicator->create_instance)
	{
		int result = indicator->create_instance(
			&instance->indicator_state, params, point->control_dir);
		if(result)
		{
			print_error("Failed to create instance of the indicator '%s'.", indicator_name);
			kfree(instance);
			return result;
		}
	}
	
	instance->indicator = indicator;
	list_add_tail(&instance->list, &indicator->instances);

	instance->current_point = point;
	point->current_instance = instance;
	
	return 0;
}

static void 
kedr_fsim_point_clear_indicator_internal(
	struct kedr_simulation_point* point)
{
	struct indicator_instance *instance = point->current_instance;
	
	if(!instance) return;

	rcu_assign_pointer(point->current_instance, NULL);
	synchronize_rcu();
	indicator_instance_destroy(instance);
}
///////////////////////////Files operations///////////////////////
static char* point_indicator_file_get_str(struct inode* inode);
static int point_indicator_file_set_str(const char* str, struct inode* inode);

CONTROL_FILE_OPS(point_indicator_file_operations, 
	point_indicator_file_get_str, point_indicator_file_set_str);

static char* point_format_string_file_get_str(struct inode* inode);

CONTROL_FILE_OPS(point_format_string_file_operations, 
	point_format_string_file_get_str, NULL);


static char* last_fault_file_get_str(struct inode* inode);
static int last_fault_file_set_str(const char* str, struct inode* inode);

CONTROL_FILE_OPS(last_fault_file_operations,
	last_fault_file_get_str, last_fault_file_set_str);


static int
create_point_files(struct kedr_simulation_point* point)
{
	point->control_dir = debugfs_create_dir(point->name, points_root_directory);
	if(point->control_dir == NULL)
	{
		print_error0("Cannot create control directory for the point.");
		goto err_control_dir;
	}

	point->indicator_file = debugfs_create_file("current_indicator",
		S_IRUGO | S_IWUSR | S_IWGRP,
		point->control_dir,
		point, &point_indicator_file_operations);
	if(point->indicator_file == NULL)
	{
		print_error0("Cannot create indicator file for the fault simulation point.");
		goto err_indicator_file;
	}

	point->format_string_file = debugfs_create_file("format_string", 
		S_IRUGO,
		point->control_dir,
		point, &point_format_string_file_operations);
	if(point->format_string_file == NULL)
	{
		print_error0("Cannot create format string file for the point.");
		goto err_format_string_file;
	}

	return 0;

err_format_string_file:
	debugfs_remove(point->indicator_file);
err_indicator_file:
	debugfs_remove(point->control_dir);
err_control_dir:

	return -EINVAL;
}

static void
delete_point_files(struct kedr_simulation_point* point)
{
	//mark opened instances of file as invalide
	point->format_string_file->d_inode->i_private = NULL;
	
	debugfs_remove(point->format_string_file);

	//mark opened instances of indicator file as invalid
	point->indicator_file->d_inode->i_private = NULL;
   
	debugfs_remove(point->indicator_file);

	debugfs_remove(point->control_dir);
}

/*
 * Create directory for indicator
 */

static int
create_indicator_files(struct kedr_simulation_indicator* indicator)
{
	indicator->control_dir = debugfs_create_dir(indicator->name, indicators_root_directory);
	if(indicator->control_dir == NULL)
	{
		print_error0("Cannot create control directory for the indicator.");
		return -1;
	}
	return 0;
}

static void
delete_indicator_files(struct kedr_simulation_indicator* indicator)
{
	debugfs_remove(indicator->control_dir);
}
/////////////////////////////////////////////////////////////////////////////

static int __init
kedr_fault_simulation_init(void)
{
	root_directory = debugfs_create_dir("kedr_fault_simulation", NULL);
	if(root_directory == NULL)
	{
		print_error0("Cannot create root directory in debugfs for service.");
		goto err_root_dir;
	}
	points_root_directory = debugfs_create_dir("points", root_directory);
	if(points_root_directory == NULL)
	{
		print_error0("Cannot create directory in debugfs for points.");
		goto err_points_dir;
	}
	indicators_root_directory = debugfs_create_dir("indicators", root_directory);
	if(indicators_root_directory == NULL)
	{
		print_error0("Cannot create directory in debugfs for indicators.");
		goto err_indicators_dir;
	}
	
	last_fault_file = debugfs_create_file("last_fault", 
		S_IRUGO,
		root_directory,
		NULL, &last_fault_file_operations);
	if(last_fault_file == NULL)
	{
		print_error0("Cannot create 'last_fault' file in debugfs.");
		goto err_last_fault_file;
	}
	
	return 0;

err_last_fault_file:
	debugfs_remove(indicators_root_directory);
err_indicators_dir:
	debugfs_remove(points_root_directory);
err_points_dir:
	debugfs_remove(root_directory);
err_root_dir:
	return -EINVAL;
}

static void
kedr_fault_simulation_exit(void)
{
	BUG_ON(!list_empty(&points));
	BUG_ON(!list_empty(&indicators));

	debugfs_remove(last_fault_file);
	debugfs_remove(points_root_directory);
	debugfs_remove(indicators_root_directory);
	debugfs_remove(root_directory);
}
module_init(kedr_fault_simulation_init);
module_exit(kedr_fault_simulation_exit);

/////////////////Setters and getters for file operations/////////////////////
static char *
point_indicator_file_get_str(struct inode* inode)
{
	char* str;
	struct kedr_simulation_point* point;
   
	if(mutex_lock_killable(&fsim_mutex))
	{
		return NULL;
	}

	point = inode->i_private;
	if(point)
	{
		struct indicator_instance* instance = point->current_instance;
		str = kstrdup(instance ? instance->indicator->name : indicator_name_not_set,
				GFP_KERNEL);
	}
	else
	{
		str = NULL;//'device', corresponed to file, is not exist
	}
	mutex_unlock(&fsim_mutex);
	
	return str;
}
static int
point_indicator_file_set_str(const char* str, struct inode* inode)
{
	struct kedr_simulation_point* point;
	char* indicator_name;
	const char* indicator_name_start, *indicator_name_end;
	const char* params;

	int error;
	// Split written string into indicator name and params
	indicator_name_start = str;

	//trim leading spaces from indicator name
	while(isspace(*indicator_name_start)) indicator_name_start++;
	//look for the end of indicator name
	indicator_name_end = indicator_name_start;
	while((*indicator_name_end != '\0') && !isspace(*indicator_name_end)) indicator_name_end++;
	//trim leading spaces from params
	params = indicator_name_end;
	while(isspace(*params)) params++;

	indicator_name = kstrndup(indicator_name_start,
		indicator_name_end - indicator_name_start, GFP_KERNEL);
	if(indicator_name == NULL)
	{
		pr_err("Cannot allocate indicator name.\n");
		return -ENOMEM;
	}

	if(mutex_lock_killable(&fsim_mutex))
	{
		kfree(indicator_name);
		return -EINTR;
	}

	point = inode->i_private;
	if(point)
	{
		if(strcmp(indicator_name, indicator_name_not_set) == 0)
		{
			kedr_fsim_point_clear_indicator_internal(point);
			error = 0;
		}
		else
		{
			error = kedr_fsim_point_set_indicator_internal(
				point, indicator_name, params);
		}
	}
	else
	{
		error = -EINVAL;
	}
	mutex_unlock(&fsim_mutex);
	kfree(indicator_name);
	
	return error;
}

static char *
point_format_string_file_get_str(struct inode* inode)
{
	char* str;
	struct kedr_simulation_point* point;
   
	if(mutex_lock_killable(&fsim_mutex))
	{
		return NULL;
	}

	point = inode->i_private;
	if(point)
	{
		str = kstrdup(point->format_string, GFP_KERNEL);
	}
	else
	{
		str = NULL; //'device' corresponding to the file does not exist
	}
	mutex_unlock(&fsim_mutex);
	
	return str;
}

static char *
last_fault_file_get_str(struct inode* inode)
{
	unsigned long flags;

	char* str = kmalloc(KEDR_FSIM_FAULT_MESSAGE_LEN + 1, GFP_KERNEL);
	
	if(str == NULL) return NULL;
	
	spin_lock_irqsave(&kedr_fsim_fault_message_lock, flags);

	strncpy(str, kedr_fsim_fault_message_buf, KEDR_FSIM_FAULT_MESSAGE_LEN);
	str[KEDR_FSIM_FAULT_MESSAGE_LEN] = '\0';
	
	spin_unlock_irqrestore(&kedr_fsim_fault_message_lock, flags);
	
	return str;
}

static int
last_fault_file_set_str(const char* str, struct inode* inode)
{
	kedr_fsim_fault_message("%s", str);
	
	return 0;
}
