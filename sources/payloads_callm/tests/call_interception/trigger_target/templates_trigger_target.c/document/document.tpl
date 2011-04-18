/* ========================================================================
 * Copyright (C) 2010-2011, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/string.h> /* strcmp() */

char* function_name;
module_param(function_name, charp, S_IRUGO);

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

<$if user_space_access$>#include <linux/uaccess.h>
#include <linux/fs.h> /*file operations*/
#include <linux/cdev.h> /*character device definition*/
#include <linux/device.h> /*class_create*/
#include <linux/err.h>

<$endif$><$if concat(header)$><$header: join(\n)$>
<$endif$>

<$if user_space_access$>enum trigger_type
{
	trigger_type_common = 0,
	trigger_type_copy_to_user,
	trigger_type_copy_from_user
};

<$endif$>struct trigger
{
	const char* function_name;
<$if user_space_access$>	enum trigger_type type;
	union
	{
		void (*trigger_function)(void);
		void (*trigger_function_copy_to_user)(void* __user user_area);
		void (*trigger_function_copy_from_user)(const void* __user user_area);
	};
	size_t user_area_size;
<$else$>	union
	{
		void (*trigger_function)(void);
	};
<$endif$>};

<$if concat(function.name)$><$block : join(\n)$>
<$endif$>

<$if user_space_access$>#include <linux/uaccess.h>
#include <linux/fs.h> /*file operations*/
#include <linux/cdev.h> /*character device definition*/
#include <linux/device.h> /*class_create*/
#include <linux/err.h>

struct tt_dev
{
	struct cdev cdev;
	//char data[10];
	//struct mutex mutex;
};

const struct trigger* current_trigger = NULL;

int 
ttd_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

ssize_t 
ttd_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	if((current_trigger == NULL)
		|| (current_trigger->type != trigger_type_copy_to_user)
		|| (current_trigger->user_area_size != count))
		return 0;//eof
		
	current_trigger->trigger_function_copy_to_user(buf);

	return count;
}

ssize_t 
ttd_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
	if((current_trigger == NULL)
		|| (current_trigger->type != trigger_type_copy_from_user)
		|| (current_trigger->user_area_size != count))
		return -EINVAL;

	current_trigger->trigger_function_copy_from_user(buf);
	
	return count;
}

struct file_operations ttd_ops =
{
	.owner = THIS_MODULE,
	.open = ttd_open,
	.read = ttd_read,
	.write = ttd_write
};

struct tt_dev dev;
struct class* ttd_class;
struct device* ttd_dev;

int ttd_minor = 0;
int ttd_major;

int ttd_init(void)
{
	int result;
	dev_t devno = 0;
	
	//memset(dev.data, 0, sizeof(dev.data));
	//mutex_init(&dev.mutex);
	
	result = alloc_chrdev_region(&devno, ttd_minor, 1, "ttd");
	if(result)
	{
		pr_err("Cannot register character device region.");
		return result;
	}
	ttd_major = MAJOR(devno);
	
	cdev_init(&dev.cdev, &ttd_ops);
	dev.cdev.owner = THIS_MODULE;
	dev.cdev.ops = &ttd_ops;
	
	ttd_class = class_create(THIS_MODULE, "ttd");
	if(IS_ERR(ttd_class))
	{
		pr_err("Cannot create class.");
		unregister_chrdev_region(devno, 1);
		return PTR_ERR(ttd_class);
	}
	
	result = cdev_add(&dev.cdev, devno, 1);
	if(result)
	{
		pr_err("Cannot add character device.");
		class_destroy(ttd_class);
		unregister_chrdev_region(devno, 1);
		return result;
	}
	ttd_dev = device_create(ttd_class, NULL, devno, NULL, "ttd");
	if(IS_ERR(ttd_dev))
	{
		pr_err("Cannot create file for added character device");
		cdev_del(&dev.cdev);
		class_destroy(ttd_class);
		unregister_chrdev_region(devno, 1);
		return PTR_ERR(ttd_dev);
	}

	return 0;
}

void ttd_destroy(void)
{
	dev_t devno = MKDEV(ttd_major, ttd_minor);;
	device_destroy(ttd_class, devno);
	cdev_del(&dev.cdev);
	class_destroy(ttd_class);
	unregister_chrdev_region(devno, 1);
}
<$endif$>

struct trigger triggers[] =
{
	<$if concat(function.name)$><$trigger: join(\n\t)$>
	<$endif$>{.function_name = NULL}
};

int __init
trigger_target_init(void)
{
	const struct trigger* trigger;
	if(function_name == NULL)
	{
		pr_err("'function_name' parameter should be set for module when it is inserted.");
		return -EINVAL;
	}
	for(trigger = triggers; trigger->function_name != NULL; trigger++)
	{
		if(strcmp(trigger->function_name, function_name) == 0)
			break;
	}
	if(trigger->function_name == NULL)
	{
		pr_err("Trigger is not exist for function '%s'.", function_name);
		return -EINVAL;
	}
	
<$if user_space_access$>	current_trigger = trigger;
	if(current_trigger->type != trigger_type_common)
	{
		return ttd_init();
	}
<$endif$>	trigger->trigger_function();
	return 0;
}
void
trigger_target_exit(void)
{
<$if user_space_access$>	if(current_trigger->type != trigger_type_common)
	{
		ttd_destroy();
	}
<$endif$>}

module_init(trigger_target_init);
module_exit(trigger_target_exit);
