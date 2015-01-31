/*
 * The "target detector" component of KEDR system. 
 * Its main responsibility is to detect when
 * target module is loaded and unloaded.
 */
 
/* ========================================================================
 * Copyright (C) 2012-2015, KEDR development team
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
};

/* Array of targets. */
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
    
    new_size = sizeof(*arr_new) * (ta->n + 1);
    arr_new = krealloc(ta->arr, new_size, GFP_KERNEL);
    if(!arr_new)
    {
        kfree(target_name);
        return -ENOMEM;
    }
   
    arr_new[ta->n].name = target_name;
    arr_new[ta->n].m = NULL;
    
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

/* Global array of targets */
static TARGET_ARRAY(targets);

/* Protect 'targets' array  from concurrent access. */
static DEFINE_MUTEX(target_mutex);

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

int kedr_target_detector_set_target_name(const char* target_name)
{
    int result = 0;
	int i;
    /* 
     * Temporary array of targets.
     * It will be assigned to global one after being checked.
     */
    TARGET_ARRAY(targets_new);
    
    int can_use_locks = param_set_can_use_lock();
    
    if(can_use_locks) mutex_lock(&target_mutex);
    
    for(i = 0; i < targets.n; i++)
    {
		if(targets.arr[i].m)
		{
			kedr_err0("Cannot change target while it is loaded");
			result = -EBUSY;
			goto fail;
		}
	}
    
    {
        static const char* delims = ",;\n";
        const char* beg = target_name;
        
        mutex_lock(&module_mutex);
        while(*beg)
        {
            size_t len = strcspn(beg, delims);
            if(len)
            {
                result = target_array_add_target(&targets_new, beg, len);
                if(result) break;
                
                if(find_module(targets_new.arr[targets_new.n - 1].name))
                {
                    kedr_err("Cannot add target name '%s' because corresponded module is currently loaded.",
                        targets_new.arr[targets_new.n - 1].name);
                    result = -EINVAL;
                    break;
                }
            }
            
            beg += len;
            if(*beg) beg++;
        }

        mutex_unlock(&module_mutex);

        if(result) goto fail;
        
        if(targets_new.n > 1)
        {
            result = force_several_targets();
            if(result) goto fail;
        }
        else
        {
            unforce_several_targets();
        }
    }

    // Set global 'targets' array
    target_array_clear(&targets);
    
    targets.arr = targets_new.arr;
    targets.n = targets_new.n;

    if(can_use_locks) mutex_unlock(&target_mutex);
    return 0;

fail:
    if(can_use_locks) mutex_unlock(&target_mutex);

    target_array_clear(&targets_new);

    return result;
}

/* ================================================================== */
/* 
 * Whether there is an target which is currently being initialized.
 * 
 * Because of Linux kernel's implementation, at most one target module
 * may have that property.
 */
int target_in_init = 0;

int kedr_target_module_in_init(void)
{
    return target_in_init;
}

/* ================================================================== */
/* 
 * A callback function to catch loading and unloading of module.
 * 
 * Sets 'target_in_init' among other things.
 */
static int 
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module* mod = (struct module *)vmod;
    int i;
	BUG_ON(mod == NULL);
	
	/* handle module state change */
	switch(mod_state)
	{
	case MODULE_STATE_COMING: /* the module has just loaded */
		mutex_lock(&target_mutex);
        
        for(i = 0; i < targets.n; i++)
        {
            if(targets.arr[i].m) continue;
            
            if(!strncmp(module_name(mod), targets.arr[i].name, MODULE_NAME_LEN))
            {
                if(!on_target_load(mod))
                {
                    targets.arr[i].m = mod;
                    target_in_init = 1;
                }
                break;
            }
        }
        
		mutex_unlock(&target_mutex);
	break;
	case MODULE_STATE_LIVE:
        /* 
         * At most one module may leave initialization stage.
         * So, it is safe to reset 'target_in_init' unconditionally.
         */
        target_in_init = 0;
    break;
	case MODULE_STATE_GOING: /* the module is going to unload */
		mutex_lock(&target_mutex);
        for(i = 0; i < targets.n; i++)
        {
            if(mod == targets.arr[i].m)
            {
                on_target_unload(mod);
                targets.arr[i].m = NULL;
                /* 
                 * If module initialization fails, no MODULE_STATE_LIVE
                 * notification will be emited.
                 * So, reset 'target_in_init' here.
                 */
                target_in_init = 0;
                break;
            }
        }

		mutex_unlock(&target_mutex);
	break;
	}

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
	
	if(result)
	{
		/*
		 * Targets array may be set before initialization.
		 * Clear it in case of error.
		 */
		target_array_clear(&targets);

	}

	return result;
}

void
kedr_target_detector_destroy(void)
{
	unregister_module_notifier(&detector_nb);

    target_array_clear(&targets);
}
