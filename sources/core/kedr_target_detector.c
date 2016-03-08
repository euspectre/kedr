/*
 * The "target detector" component of KEDR system. 
 * Its main responsibility is to detect when
 * target module is loaded and unloaded.
 */
 
/* ========================================================================
 * Copyright (C) 2012-2015, KEDR development team
 * Copyright (C) 2010-2012, Institute for System Programming 
 *						  of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *	  Eugene A. Shatokhin <spectre@ispras.ru>
 *	  Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h> /* 'struct module' definition */

#include <linux/kernel.h>
#include <linux/mutex.h>

#include <linux/slab.h>

#include "kedr_internal.h"
#include "kedr_target_detector_internal.h"

#include <kedr/core/kedr.h>
/* ================================================================ */
/* This string will be used in debug output to specify the name of 
 * the current component of KEDR
 */
#define COMPONENT_STRING "kedr_target_detector: "

/* ================================================================ */
/* Descriptor for single target. */
struct target_struct
{
	/* Name of the target. */
	char* name;
	/* Module corresponded to given name, if loaded. Otherwise NULL. */
	struct module* m;
	/* Whether given module executes its 'init' section. */
	int in_init;
};

/* Array of targets with unique names. */
struct target_array
{
	struct target_struct* arr;
	int n;
};

// Declare and initialize array of targets.
#define TARGET_ARRAY(name) struct target_array name = {NULL, 0}


/* 
 * Add target with given name to the array.
 * 
 * NOTE: 'm' field for the target is set to NULL.
 */
static int target_array_add_target(struct target_array* ta,
	const char* name, size_t name_len)
{
	char* target_name;
	int i;
	struct target_struct* arr_new;
	size_t new_size;

	target_name = kmalloc(name_len + 1, GFP_KERNEL);
	if(!target_name) return -ENOMEM;
	
	memcpy(target_name, name, name_len);
	// Replace dashes with undescores
	for(i = 0; i < name_len; i++)
	{
		if(target_name[i] == '-') target_name[i] = '_';
	}
	target_name[name_len] = '\0';

	// Check name uniqueness.
	for(i = 0; i < ta->n; i++)
	{
		if(!strcmp(target_name, ta->arr[i].name))
		{
			// Just ignore that addition.
			kfree(target_name);
			return 0;
		}
	}

	
	new_size = sizeof(*arr_new) * (ta->n + 1);
	arr_new = krealloc(ta->arr, new_size, GFP_KERNEL);
	if(!arr_new)
	{
		kfree(target_name);
		return -ENOMEM;
	}
   
	arr_new[ta->n].name = target_name;
	arr_new[ta->n].m = NULL;
	arr_new[ta->n].in_init = 0;
	
	ta->arr = arr_new;
	ta->n++;
	
	return 0;
}

/* Clear targets array. */
static void target_array_clear(struct target_array* ta)
{
	int i;
	for(i = 0; i < ta->n; i++)
	{
		kfree(ta->arr[i].name);
	}
	kfree(ta->arr);
	
	ta->arr = NULL;
	ta->n = 0;
}

/* 
 * Move targets from `ta_src` array to `ta_dst`.
 */
static void target_array_move(struct target_array* ta_dst,
	struct target_array* ta_src)
{
	target_array_clear(ta_dst);
	
	ta_dst->arr = ta_src->arr;
	ta_dst->n = ta_src->n;
	
	ta_src->arr = NULL;
	ta_src->n = 0;
}

/* Global array of targets */
static TARGET_ARRAY(targets);

/* Whether module notifier is registered. 
 * 
 * Until module notifier is registered, it is unreliable to check
 * whether module with given name is loaded.
 * 
 * This variable is set only during initialization, so it does not
 * require locking rules: interfaces are not allowed to be used *during*
 * initialization.
 */
int is_module_notifier_registered = 0;

/* Global array of targets *for being set*. */
static TARGET_ARRAY(targets_pending);

/* 
 * Protect 'targets' and `targets_pending` arrays from concurrent
 * access.
 */
static DEFINE_MUTEX(target_mutex);

/* Counter of targets in init section. */
static atomic_t target_init_counter = ATOMIC_INIT(0);

/**************** Writer into buffer of limited length ****************/
struct buffer_writer
{
	char* p;
	int max_size;
	int size;
};

#define BUFFER_WRITER(name, p, max_size) struct buffer_writer name = {p, max_size, 0}

/* Write 'bytes' array of length 'n' into buffer. Exceeded characters are discarded. */
static inline void buffer_write_bytes(struct buffer_writer* bw,
	const char* bytes, int n)
{
	if(bw->max_size != bw->size)
	{
		int n_real = bw->size + n < bw->max_size ? n : bw->max_size - bw->size;
		memcpy(bw->p + bw->size, bytes, n_real);
		bw->size += n_real;
	}
}

/* Write character into buffer. If buffer is full, do nothing. */
static inline void buffer_write_char(struct buffer_writer* bw, char c)
{
	if(bw->max_size != bw->size)
	{
		bw->p[bw->size] = c;
		bw->size++;
	}
}

/*********** Implementation of 'target' module parameter **************/
int kedr_target_detector_get_target_name(char* buf, size_t size)
{
	int i;
	BUFFER_WRITER(bw, buf, size);
	
	mutex_lock(&target_mutex);

	for(i = 0; i < targets.n; i++)
	{
		const char* name;
		int len;
		
		if(i) buffer_write_char(&bw, ',');
		
		name = targets.arr[i].name;
		len = strlen(name);
		
		buffer_write_bytes(&bw, name, len);
	}
	
	mutex_unlock(&target_mutex);
	
	return bw.size;
}

/* 
 * Set 'targets' according to value of 'targets_pending' with checks.
 * 
 * Can be called only when 'is_module_notifier_registered' is true.
 * 
 * Should be called with 'target_mutex' locked.
 */
static int set_target_name_internal(void)
{
	int i;
	int result = 0;
	BUG_ON(!is_module_notifier_registered);
	
	for(i = 0; i < targets.n; i++)
	{
		if(targets.arr[i].m)
		{
			kedr_err0("Cannot change target while it is loaded");
			return -EBUSY;
		}
	}
	
	mutex_lock(&module_mutex); // Need for find_module() call.
	for(i = 0; i < targets_pending.n; i++)
	{
		const char* target_name = targets_pending.arr[i].name;
		
		if(find_module(target_name))
		{
			kedr_err("Cannot add target name '%s' because corresponded module is currently loaded.",
				target_name);
			result = -EINVAL;
			break;
		}
		
	}
	mutex_unlock(&module_mutex);
	if(result) return result;
		
	if(targets_pending.n > 1)
	{
		result = force_several_targets();
		if(result) return result;
	}
	else
	{
		unforce_several_targets();
	}
	
	target_array_move(&targets, &targets_pending);

	return 0;
}

int kedr_target_detector_set_target_name(const char* target_name)
{
	int result = 0;
	static const char* delims = ",;\n";
	const char* beg = target_name;

	int can_use_locks = param_set_can_use_lock();

	if(can_use_locks) mutex_lock(&target_mutex);

	/* Unconditionally replace 'targets_pending' array. */
	target_array_clear(&targets_pending);

	while(*beg)
	{
		size_t len = strcspn(beg, delims);
		if(len)
		{
			result = target_array_add_target(&targets_pending, beg, len);
			if(result) goto fail;
		}

		beg += len;
		if(*beg) beg++;
	}

	if(is_module_notifier_registered)
	{
		result = set_target_name_internal();
		if(result) goto fail;
	}

	if(can_use_locks) mutex_unlock(&target_mutex);
	return 0;

fail:
	target_array_clear(&targets_pending);

	if(can_use_locks) mutex_unlock(&target_mutex);

	return result;
}

/* ================================================================== */

int kedr_target_module_in_init(void)
{
	return atomic_read(&target_init_counter) != 0;
}

/* ================================================================== */
/* 
 * A callback function to catch loading and unloading of module.
 * 
 * Update 'target_init_counter' among other things.
 */
static int 
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module* mod = (struct module *)vmod;
	struct target_struct* target = NULL;
	int i;
	BUG_ON(mod == NULL);
	
	mutex_lock(&target_mutex);

	for(i = 0; i < targets.n; i++)
	{
		target = &targets.arr[i];
		
		/*
		 * Transitions of *already loaded* target are not tracked,
		 * if its loading hasn't be detected.
		 * 
		 * NOTE: In the current KEDR core implementation such situation
		 * is impossible.
		 * 
		 * Also this filter is cheaper than strncmp() one in case
		 * of non-target module is unloaded.
		 */
		if(!target->m && (mod_state != MODULE_STATE_COMING)) continue;
		
		if(!strncmp(module_name(mod), target->name, MODULE_NAME_LEN))
			break;
	}
	
	if(i == targets.n) goto out; // Not a target.

	/* handle module state change */
	switch(mod_state)
	{
	case MODULE_STATE_COMING: /* the module has just loaded */
		if(on_target_load(mod)) goto out;
		target->m = mod;
		target->in_init = 1;
		atomic_inc(&target_init_counter);
	break;
	case MODULE_STATE_LIVE: /* the module has just initialized */
		target->in_init = 0;
		atomic_dec(&target_init_counter);
	break;
	case MODULE_STATE_GOING: /* the module is going to unload */
		/* 
		 * This state can come just after initialization
		 * (e.g. if that initialization fails), so care with
		 * 'target_init_counter'.
		 * 
		 * NOTE: This is the only reason for 'in_init' field in
		 * 'target_struct'.
		 */
		on_target_unload(mod);
		if(target->in_init)
		{
			target->in_init = 0;
			atomic_dec(&target_init_counter);
		}
		target->m = NULL;
	break;
	}

out:
	mutex_unlock(&target_mutex);
	return 0;
}

/* ================================================================ */
/* A struct for watching for loading/unloading of modules.*/
static struct notifier_block detector_nb = {
	.notifier_call = detector_notifier_call,
	.next = NULL,
	
	/* Let others (Ftrace, etc.) do their job first. Not strictly 
	 * necessary here but makes no harm. */
	.priority = -1, 
};

/* ================================================================ */
int
kedr_target_detector_init(void)
{
	int result = 0;
	KEDR_MSG(COMPONENT_STRING "Initializing\n");

	result = register_module_notifier(&detector_nb);
	
	if(result) goto err;

	mutex_lock(&target_mutex);
	is_module_notifier_registered = 1;
	result = set_target_name_internal();
	mutex_unlock(&target_mutex);

	if(result) goto err_set_target;
	
	return 0;

err_set_target:
	unregister_module_notifier(&detector_nb);
err:
	/*
	 * Pending targets array may be set before initialization.
	 * Clear it in the case of error.
	 */
	target_array_clear(&targets_pending);
	return result;
}

void
kedr_target_detector_destroy(void)
{
	unregister_module_notifier(&detector_nb);

	target_array_clear(&targets);
	// Just in case
	target_array_clear(&targets_pending);
}
