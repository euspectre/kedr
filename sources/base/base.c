/*
 * The "base" component of KEDR system. 
 * "kedr-base" keeps a registry of payload modules and provides interface 
 * for them to register / unregister themselves. some .
 * It also provides convenience functions for payload modules and the interface 
 * that "kedr-controller" uses during the instrumentation of a target module.
 *
 * Copyright (C) 2010 Institute for System Programming 
 *		              of the Russian Academy of Sciences (ISPRAS)
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

	
/* [NB] For now, we just don't care of some synchronization issues.
 * */

/* TODO: protect access to target_module with a mutex.
 * 
 * */


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
	struct kedr_payload* payload;
};
/* ================================================================ */

// TODO: protect global data from race conditions

/* The list of currently loaded payload modules - 
 * effectively, the list of 'struct kedr_payload*' */
struct list_head payload_modules;

/* Nonzero if registering payloads is allowed, 0 otherwise (for example, 
 * if a target module is currently loaded) */
int deny_payload_register = 0;
/* A mutex to protect operations with the payloads */
DEFINE_MUTEX(payload_mutex);

/* The combined replacement table (from all payload modules) */
struct kedr_repl_table combined_repl_table;
/* A mutex to protect operations with the replacement table */
DEFINE_MUTEX(repl_table_mutex);

/* The current controller */
struct kedr_impl_controller* current_controller = NULL;
/* A mutex to protect operations with the controller */
DEFINE_MUTEX(controller_mutex);

/* ================================================================ */
/* Free the combined replacement table, reset the pointers to NULL and 
 * the number of elements - to 0. */
static void
free_repl_table(struct kedr_repl_table* repl_table)
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
create_repl_table(struct kedr_repl_table* repl_table)
{
	struct payload_module_list* entry;
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
	/* The system won't let unload "kedr-base" while at least one
	 * payload module is loaded: payload modules use symbols from the 
	 * "kedr-base".
	 * So, if we managed to get here, there must be no payload modules 
	 * registered.
	 *  */
	BUG_ON(!list_empty(&payload_modules));
	
	/* Even if a target module is now loaded, it must not have been
	 * instrumented as there are no payload modules at the moment.
	 * Note that a payload module cannot be unloaded if there is a 
	 * target module present. So there is no need to uninstrument 
	 * the target module: it was never instrumented. It was probably 
	 * loaded with no payload modules present. 
	 * */
	
	/* Just in case the controller failed to unregister itself */
	kedr_impl_controller_unregister(current_controller);
	
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
payload_find(struct kedr_payload* payload)
{
	struct payload_module_list* entry;
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
kedr_payload_register(struct kedr_payload* payload)
{
	struct payload_module_list* new_elem = NULL;
	
	BUG_ON(payload == NULL);
	
	/* If there is a target module already watched for, do not allow
	 * to register another payload. */
	if (deny_payload_register)
	{
		return -EBUSY;
	}
	
	if (payload_find(payload) != NULL)
	{
		KEDR_MSG(COMPONENT_STRING
			"module \"%s\" attempts to register the same payload twice\n",
			module_name(payload->mod));
		return -EINVAL;
	}
	
	KEDR_MSG(COMPONENT_STRING
		"registering payload from module \"%s\"\n",
		module_name(payload->mod));
	
	new_elem = kzalloc(sizeof(struct payload_module_list), GFP_KERNEL);
	if (new_elem == NULL) return -ENOMEM;
		
	INIT_LIST_HEAD(&new_elem->list);
	new_elem->payload = payload;
	
	list_add_tail(&new_elem->list, &payload_modules);
	return 0;
}
EXPORT_SYMBOL(kedr_payload_register);

void 
kedr_payload_unregister(struct kedr_payload* payload)
{
	struct payload_module_list* doomed = NULL;
	BUG_ON(payload == NULL);
	
	doomed = payload_find(payload);
	if (doomed == NULL)
	{
		KEDR_MSG(COMPONENT_STRING
			"module \"%s\" attempts to unregister the payload "
			"that was never registered\n",
			module_name(payload->mod));
		return;
	}
	
	KEDR_MSG(COMPONENT_STRING
		"unregistering payload from module \"%s\"\n",
		module_name(payload->mod));
	
	list_del(&doomed->list);
	kfree(doomed);
	return;
}
EXPORT_SYMBOL(kedr_payload_unregister);

int
kedr_target_module_in_init(void)
{
	/* The call should be delegated to the controller.
	 * The caller must ensure the controller is present.
	 */
	BUG_ON(current_controller == NULL);
	
	/* TODO!!! */
	return 0; 
}
EXPORT_SYMBOL(kedr_target_module_in_init);

/**********************************************************************/
int 
kedr_impl_controller_register(struct kedr_impl_controller* controller)
{
	int result = 0;
	BUG_ON(
		controller == NULL || 
		controller->mod == NULL ||
		controller->delegates.target_module_in_init == NULL
	);
	
	result = mutex_lock_interruptible(&controller_mutex);
	if (result != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock controller_mutex\n");
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
	mutex_unlock(&controller_mutex); 
	return result;
}
EXPORT_SYMBOL(kedr_impl_controller_register);

void
kedr_impl_controller_unregister(struct kedr_impl_controller* controller)
{
	if (mutex_lock_interruptible(&controller_mutex) != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock controller_mutex\n");
		return;
	}
	
	if (current_controller != NULL)
	{
		/* 'controller' is allowed to be NULL in one situation only:
		 * if kedr_impl_controller_unregister(current_controller) is called 
		 * from the cleanup function of kedr-base and current_controller 
		 * is NULL.
		 */ 
		BUG_ON(controller == NULL);
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
	mutex_unlock(&controller_mutex);
	return;
}
EXPORT_SYMBOL(kedr_impl_controller_unregister);

const struct kedr_repl_table*  
kedr_impl_get_repl_table(void)
{
	/*TODO*/
	return NULL;
}
EXPORT_SYMBOL(kedr_impl_get_repl_table);

void
kedr_impl_target_loaded(void)
{
	/*TODO*/
	return;
}
EXPORT_SYMBOL(kedr_impl_target_loaded);

void
kedr_impl_target_unloaded(void)
{
	/*TODO*/
	return;
}
EXPORT_SYMBOL(kedr_impl_target_unloaded);
/* ================================================================ */
