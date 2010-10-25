/*
 * The "base" component of KEDR system. 
 * "kedr-base" keeps a registry of payload modules and provides interface 
 * for them to register / unregister themselves. some .
 * It also provides convenience functions for payload modules and the interface 
 * that "kedr-controller" uses during the instrumentation of a target module.
 *
 * Copyright (C) 2010 Institute for System Programming 
 *                    of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *		Eugene A. Shatokhin <spectre@ispras.ru>
 *		Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/list.h>

#include <linux/mutex.h>

#include <kedr/base/common.h>

/* ================================================================ */
/* This string will be used in debug output to specify the name of 
 * the current component of KEDR
 */
#define COMPONENT_STRING "base: "

/* ================================================================ */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ================================================================ */

struct payload_module_list
{
	struct list_head list;
	struct kedr_payload *payload;
};
/* ================================================================ */

/* The list of currently loaded payload modules - 
 * effectively, the list of 'struct kedr_payload*' */
struct list_head payload_modules;

/* Nonzero if registering payloads is allowed, 0 otherwise (for example, 
 * if a target module is currently loaded) */
int deny_payload_register = 0;

/* The combined replacement table (from all payload modules) */
struct kedr_repl_table combined_repl_table;

/* The current controller */
struct kedr_impl_controller *current_controller = NULL;

/* A mutex to protect access to the global data of kedr-base*/
DEFINE_MUTEX(base_mutex);

/* ================================================================ */
/* Free the combined replacement table, reset the pointers to NULL and 
 * the number of elements - to 0. */
static void
free_repl_table(struct kedr_repl_table *repl_table)
{
	BUG_ON(repl_table == NULL);
	
	kfree(repl_table->orig_addrs);
	kfree(repl_table->repl_addrs);
	
	repl_table->orig_addrs = NULL;
	repl_table->repl_addrs = NULL;
	repl_table->num_addrs = 0;
	return;
}

/* Allocate appropriate amount of memory and combine the replacement 
 * tables from all payload modules into a single 'table'. Actually, 
 * the table is returned in two arrays, '*ptarget_funcs' and '*repl_funcs',
 * the number of elements in each one is returned in '*pnum_funcs'.
 * */
static int
create_repl_table(struct kedr_repl_table *repl_table)
{
	struct payload_module_list *entry;
	struct list_head *pos;
	unsigned int i;
	
	BUG_ON(repl_table == NULL);
	
	repl_table->orig_addrs = NULL;
	repl_table->repl_addrs = NULL;
	repl_table->num_addrs = 0;
	
	/* Determine the total number of target functions. If there are no
	 * target functions, do nothing. No need to allocate memory in this 
	 * case.
	 * */
	list_for_each(pos, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
		repl_table->num_addrs += entry->payload->repl_table.num_addrs;
	}
	
	KEDR_MSG(COMPONENT_STRING 
		"total number of target functions is %u.\n",
		repl_table->num_addrs);
		
	repl_table->orig_addrs = kzalloc(
		repl_table->num_addrs * sizeof(void*), 
		GFP_KERNEL);
	repl_table->repl_addrs = kzalloc(
		repl_table->num_addrs * sizeof(void*), 
		GFP_KERNEL);
	if (repl_table->orig_addrs == NULL || repl_table->repl_addrs == NULL)
	{
		/* Don't care which of the two has failed, kfree(NULL) is 
		 * almost a no-op anyway*/
		free_repl_table(repl_table);
		return -ENOMEM;		
	}
	
	i = 0;
	list_for_each(pos, &payload_modules)
	{
		unsigned int k;
		struct kedr_repl_table* rtable;
			
		entry = list_entry(pos, struct payload_module_list, list);
		BUG_ON(entry->payload == NULL);
		
		rtable = &(entry->payload->repl_table);
		BUG_ON( rtable->orig_addrs == NULL || 
				rtable->repl_addrs == NULL);
		
		for (k = 0; k < rtable->num_addrs; ++k)
		{
			repl_table->orig_addrs[i] = rtable->orig_addrs[k];
			repl_table->repl_addrs[i] = rtable->repl_addrs[k];
			++i;
		}
	}
	
	return 0;
}
/* ================================================================ */

static void
base_cleanup_module(void)
{
	/* If some payload modules failed to unregister themselves by now,
	 * give warnings and unregister them now.
	 */
	struct payload_module_list *entry;
	struct list_head *pos, *tmp;
	list_for_each_safe(pos, tmp, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
		KEDR_MSG(COMPONENT_STRING
			"payload module \"%s\" did not unregister itself, "
			"unregistering it now\n",
			module_name(entry->payload->mod));
		kedr_payload_unregister(entry->payload);
	}
	
	/* Just in case the controller failed to unregister itself.
	 * The controller must have been already unloaded anyway.
	 */
	kedr_impl_controller_unregister(current_controller);
	
	/* Free the replacement table */
	free_repl_table(&combined_repl_table);
	
	KEDR_MSG(COMPONENT_STRING
		"cleanup successful\n");
	return;
}

/* ================================================================ */
static int __init
base_init_module(void)
{
	KEDR_MSG(COMPONENT_STRING
		"initializing\n");

	/* Initialize the list of payloads */
	INIT_LIST_HEAD(&payload_modules);	
	return 0; 
}

static void __exit
base_exit_module(void)
{
	base_cleanup_module();
	return;
}

module_init(base_init_module);
module_exit(base_exit_module);

/* ================================================================ */

/* Look for a given element in the list. */
static struct payload_module_list* 
payload_find(struct kedr_payload *payload)
{
	struct payload_module_list *entry;
	struct list_head *pos;
	
	list_for_each(pos, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
		if(entry->payload == payload)
			return entry;
	}
	return NULL;
}

/* ================================================================ */
/* Implementation of public API                                     */
/* ================================================================ */

int 
kedr_payload_register(struct kedr_payload *payload)
{
	int result = 0;
	struct payload_module_list *new_elem = NULL;
	
	BUG_ON(payload == NULL);
    
	result = mutex_lock_interruptible(&base_mutex);
	if (result != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock base_mutex\n");
		return result;
	}
	
	/* If there is a target module already watched for, do not allow
	 * to register another payload. */
	if (deny_payload_register)
	{
		result = -EBUSY;
		goto out;
	}
	
	if (payload_find(payload) != NULL)
	{
		KEDR_MSG(COMPONENT_STRING
			"module \"%s\" attempts to register the same payload twice\n",
			module_name(payload->mod));
		result = -EINVAL;
		goto out;
	}
	
	KEDR_MSG(COMPONENT_STRING
		"registering payload from module \"%s\"\n",
		module_name(payload->mod));
	
	new_elem = kzalloc(sizeof(struct payload_module_list), GFP_KERNEL);
	if (new_elem == NULL)
	{
		result = -ENOMEM;
		goto out;
	}
		
	INIT_LIST_HEAD(&new_elem->list);
	new_elem->payload = payload;
	
	list_add_tail(&new_elem->list, &payload_modules);

out:
	mutex_unlock(&base_mutex); 	
	return result;
}
EXPORT_SYMBOL(kedr_payload_register);

void 
kedr_payload_unregister(struct kedr_payload *payload)
{
	struct payload_module_list *doomed = NULL;
	BUG_ON(payload == NULL);

	if (mutex_lock_interruptible(&base_mutex))
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock base_mutex\n");
		return;
	}
    
	doomed = payload_find(payload);
	if (doomed == NULL)
	{
		KEDR_MSG(COMPONENT_STRING
			"module \"%s\" attempts to unregister the payload "
			"that was never registered\n",
			module_name(payload->mod));
		goto out;
	}
	
	KEDR_MSG(COMPONENT_STRING
		"unregistering payload from module \"%s\"\n",
		module_name(payload->mod));
	
	list_del(&doomed->list);
	kfree(doomed);
out:
	mutex_unlock(&base_mutex);
	return;
}
EXPORT_SYMBOL(kedr_payload_unregister);

int
kedr_target_module_in_init(void)
{
	BUG_ON(current_controller == NULL);
	
/*
 * [NB] There is no need to use locking for 'current_controller' here.
 * kedr_target_module_in_init() may be called only from replacement
 * functions here. This means, the target module and hence the controller
 * module stays loaded during this call. So current_controller remains
 * meaningful.
 *
 * A spinlock is used to guarantee that the delegate is not executed several
 * times simultaneously.
 * 
 * The delegate must not sleep/reschedule but may use spinlocks.
 */
	
	/* Delegate the job to the controller */	
	return current_controller->delegates.target_module_in_init(); 
}
EXPORT_SYMBOL(kedr_target_module_in_init);

/**********************************************************************/
int 
kedr_impl_controller_register(struct kedr_impl_controller *controller)
{
	int result = 0;
	KEDR_MSG(COMPONENT_STRING
	"kedr_impl_controller_register()\n");
	
	BUG_ON(
		controller == NULL || 
		controller->mod == NULL ||
		controller->delegates.target_module_in_init == NULL
	);
	
	result = mutex_lock_interruptible(&base_mutex);
	if (result != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock base_mutex\n");
		return result;
	}
	
	if (current_controller != NULL)
	{
		KEDR_MSG(COMPONENT_STRING
			"a controller is already registered\n");
		result = -EINVAL;
		goto out;
	}
	
	current_controller = controller;
	KEDR_MSG(COMPONENT_STRING
			"controller has been registered successfully\n");
out:
	mutex_unlock(&base_mutex); 
	return result;
}
EXPORT_SYMBOL(kedr_impl_controller_register);

void
kedr_impl_controller_unregister(struct kedr_impl_controller *controller)
{
	KEDR_MSG(COMPONENT_STRING
	"kedr_impl_controller_unregister()\n");
	
	if (mutex_lock_interruptible(&base_mutex) != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock base_mutex\n");
		return;
	}
	
	if (current_controller != NULL)
	{
		/* 'controller' is allowed to be NULL in one situation only:
		 * if kedr_impl_controller_unregister(current_controller) is called 
		 * from the cleanup function of kedr-base and current_controller 
		 * is NULL.
		 */ 
		if (current_controller == controller)
		{
			current_controller = NULL;
			KEDR_MSG(COMPONENT_STRING
				"controller has been unregistered successfully\n");
		}
		else
		{
			KEDR_MSG(COMPONENT_STRING
				"an attempt was made to unregister a controller that "
				"was never registered\n");
		}
	}
	else if (controller != NULL)
	{
		KEDR_MSG(COMPONENT_STRING
			"an attempt was made to unregister a controller that "
			"was never registered\n");
	}
	mutex_unlock(&base_mutex);
	return;
}
EXPORT_SYMBOL(kedr_impl_controller_unregister);

int
kedr_impl_on_target_load(struct module *target_module, 
    struct kedr_repl_table *ptable)
{
	int result = 0;
	struct payload_module_list *entry;
	struct list_head *pos;
    struct kedr_payload *payload;
	
	KEDR_MSG(COMPONENT_STRING
	"kedr_impl_on_target_load()\n");
	
	BUG_ON(ptable == NULL);
    BUG_ON(target_module == NULL);
	
	result = mutex_lock_interruptible(&base_mutex);
	if (result != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock base_mutex\n");
		return result;
	}	
	
	deny_payload_register = 1;
	
	/* If the table has not been created, free_repl_table() is almost
	 * a no-op anyway.
	 */
	free_repl_table(&combined_repl_table);
	result = create_repl_table(&combined_repl_table);
	if (result != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to create the combined replacement table: "
			"not enough memory\n");
		goto out;
	}
	
	/* Call try_module_get for all registered payload modules to 
	 * prevent them from unloading while the target is loaded */
	list_for_each(pos, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
        payload = entry->payload;
		BUG_ON(payload->mod == NULL);
		
		KEDR_MSG(COMPONENT_STRING
		"calling try_module_get() for payload module \"%s\".\n",
			module_name(payload->mod));
		
		if (try_module_get(payload->mod) == 0)
		{
			KEDR_MSG(COMPONENT_STRING
		"try_module_get() failed for payload module \"%s\".\n",
				module_name(payload->mod));
			result = -EFAULT; 
			goto out;
		}
        
        /* Notify the payload module that the target has just loaded */
        if (payload->target_load_callback != NULL)
            (*(payload->target_load_callback))(target_module);
	}
	
	/* Make the controller stay loaded as long as the target stays loaded */
	if(try_module_get(current_controller->mod) == 0)
	{
		KEDR_MSG(COMPONENT_STRING
	"try_module_get() failed for the controller module \"%s\".\n",
			module_name(current_controller->mod));
	}
	
	ptable->orig_addrs = combined_repl_table.orig_addrs;
	ptable->repl_addrs = combined_repl_table.repl_addrs;
	ptable->num_addrs  = combined_repl_table.num_addrs;
out:
	mutex_unlock(&base_mutex); 
	return result;
}
EXPORT_SYMBOL(kedr_impl_on_target_load);

int
kedr_impl_on_target_unload(struct module *target_module)
{
	int result;
	struct payload_module_list *entry;
	struct list_head *pos;
    struct kedr_payload *payload;
	
	KEDR_MSG(COMPONENT_STRING
	"kedr_impl_on_target_unload()\n");
    
    BUG_ON(target_module == NULL);
	
	result = mutex_lock_interruptible(&base_mutex);
	if (result != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock base_mutex\n");
		return result;
	}

	/* Release the payload modules as the target is about to unload and 
	 * will execute no code from now on.
	 * 
	 * The payload modules can now unload too if the user wishes so.
	 * */
	list_for_each(pos, &payload_modules)
	{
		entry = list_entry(pos, struct payload_module_list, list);
        payload = entry->payload;
		BUG_ON(payload->mod == NULL);
		
		/* Notify the payload module that the target is about to unload */
        if (payload->target_unload_callback != NULL)
            (*(payload->target_unload_callback))(target_module);
		
		KEDR_MSG(COMPONENT_STRING
			"module_put() for payload module \"%s\".\n",
			module_name(payload->mod));
		module_put(payload->mod);
	}
	
	/* Release the controller module as well */
	module_put(current_controller->mod);
	
	/* Allow registering new payload modules */
	deny_payload_register = 0;

	mutex_unlock(&base_mutex); 
	return result;
}
EXPORT_SYMBOL(kedr_impl_on_target_unload);
/* ================================================================ */
