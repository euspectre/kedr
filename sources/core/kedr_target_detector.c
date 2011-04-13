/*
 * The "target detector" component of KEDR system. 
 * Its main responsibility is to detect when
 * target module is loaded and unloaded.
 */
 
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

/*
 * Based on the controller/controller.c from KEDR package at 21.03.11.
 * (notification part)
 */

#include <linux/module.h> /* 'struct module' definition */

#include <linux/kernel.h>
#include <linux/mutex.h>

#include <linux/slab.h>

#include "kedr_target_detector_internal.h"

#include <kedr/core/kedr.h>
/* ================================================================ */
/* This string will be used in debug output to specify the name of 
 * the current component of KEDR
 */
#define COMPONENT_STRING "kedr_target_detector: "

/* ================================================================ */
/*
 * Name of the module which loading/unloading we want to detect.
 * 
 * NULL means that name is not set.
 */
static char* target_name;

static struct kedr_target_module_notifier* notifier;

/* Target module. NULL if the module is not currently loaded. */
static struct module* target_module = NULL;
/* Protect 'target_module' and 'target_name' from concurrent access. */
static DEFINE_MUTEX(target_module_mutex);


/* ================================================================== */
/* A callback function to catch loading and unloading of module. 
 * Sets target_module pointer among other things. */
static int 
detector_notifier_call(struct notifier_block *nb,
    unsigned long mod_state, void *vmod)
{
    struct module* mod = (struct module *)vmod;
    BUG_ON(mod == NULL);
    
    /* handle module state change */
    switch(mod_state)
    {
    case MODULE_STATE_COMING: /* the module has just loaded */
        if(mutex_lock_killable(&target_module_mutex))
        {
            KEDR_MSG(COMPONENT_STRING
                "failed to lock target_module_mutex\n");
            return 0;
        }
        if((target_name != NULL)
            && (strcmp(target_name, module_name(mod)) == 0))
        {
            BUG_ON(target_module != NULL);
            if((notifier->mod == NULL) || try_module_get(notifier->mod))
            {
                if(!notifier->on_target_load(notifier, mod))
                {
                    target_module = mod;
                }
                else
                {
                    if(notifier->mod)
                        module_put(notifier->mod);
                }
            }
            else
            {
                pr_err("Fail to fix module of notifier.");
            }
        }
        mutex_unlock(&target_module_mutex);
    break;
    
    case MODULE_STATE_GOING: /* the module is going to unload */
    /* if the target module has already been unloaded, 
     * target_module is NULL, so (mod != target_module) will
     * be true. */
        mutex_lock(&target_module_mutex);
        if(mod == target_module)
        {
            notifier->on_target_unload(notifier, mod);
            target_module = NULL;
            
            if(notifier->mod != NULL)
                module_put(notifier->mod);
        }
        mutex_unlock(&target_module_mutex);
    break;
    }

    return 0;
}

/* ================================================================ */
/* A struct for watching for loading/unloading of modules.*/
static struct notifier_block detector_nb = {
    .notifier_call = detector_notifier_call,
    .next = NULL,
    .priority = 3, /*Some number*/
};

/* ================================================================ */
int
kedr_target_detector_init(struct kedr_target_module_notifier* notifier_param)
{
    int result = 0;
    KEDR_MSG(COMPONENT_STRING
        "initializing\n");
    
    notifier = notifier_param;
    target_name = NULL;
    
    target_module = NULL;
    
    result = register_module_notifier(&detector_nb);

    return result;
}

void
kedr_target_detector_destroy(void)
{
    unregister_module_notifier(&detector_nb);

    if(target_module)
    {
        pr_warning("Target module detector was unloaded while target module is loaded.");
    }
    
    kfree(target_name);
}

/* Should be executed with both module_mutex and target_module_mutex locked. */
static int set_target_name_internal(const char* name)
{
   if(target_module != NULL)
    {
        KEDR_MSG(COMPONENT_STRING
            "Cannot change name of the module to watch while module is loaded.\n");
        return -EBUSY;

    }
    /* Check if new target is already loaded */
    if ((name != NULL) && (find_module(name) != NULL))
    {
        KEDR_MSG(COMPONENT_STRING
            "target module \"%s\" is already loaded\n",
            name);
        
        KEDR_MSG(COMPONENT_STRING
    "instrumenting already loaded target modules is not supported\n");
        return -EEXIST;
    }
    
    /*
     * Currently, notifications about module state change are
     * performed after loading of the module
     * within SAME lock of 'module_mutex'(see kernel code).
     * 
     * So, setting name of the target module after verification, that
     * this module has not loaded yet within SAME lock of 'module_mutex'
     * is sufficient for enforce, that we will be correctly notified
     * about loading of this module.
     * 
     * Note: without such kernel code one cannot enforce such requirement.
     */
    kfree(target_name);
    if(name != NULL)
    {
        target_name = kstrdup(name, GFP_KERNEL);
        if(target_name == NULL)
        {
            pr_err("Failed to allocate target name string.");
            return -ENOMEM;
        }
    }
    else
    {
        target_name = NULL;
    }
    
    return 0;
}

int kedr_target_detector_set_target_name(const char* name)
{
    int result;
    
    /* 
     * Only this order of mutex locking is correct.
     * 
     * Otherwise deadlock is possible, because
     * detector_notifier_call() is called with module_mutex locked.
     */
    result = mutex_lock_killable(&module_mutex);
    if(result)
    {
        KEDR_MSG(COMPONENT_STRING
            "failed to lock module_mutex\n");
        return -EINTR;
    }
    result = mutex_lock_killable(&target_module_mutex);
    if(result)
    {
        KEDR_MSG(COMPONENT_STRING
            "failed to lock target_module_mutex\n");
        mutex_unlock(&module_mutex);
        return -EINTR;
    }
    
    result = set_target_name_internal(name);
    
    mutex_unlock(&target_module_mutex);
    mutex_unlock(&module_mutex);
    
    return result;
}

int kedr_target_detector_clear_target_name(void)
{
    int result;
    
    /* 
     * Only this order of mutex locking is correct.
     * 
     * Otherwise deadlock is possible, because
     * detector_notifier_call() is called with module_mutex locked.
     */
    result = mutex_lock_killable(&module_mutex);
    if(result)
    {
        KEDR_MSG(COMPONENT_STRING
            "failed to lock module_mutex\n");
        return -EINTR;
    }
    result = mutex_lock_killable(&target_module_mutex);
    if(result)
    {
        KEDR_MSG(COMPONENT_STRING
            "failed to lock target_module_mutex\n");
        mutex_unlock(&module_mutex);
        return -EINTR;
    }
    
    result = set_target_name_internal(NULL);
    
    mutex_unlock(&target_module_mutex);
    mutex_unlock(&module_mutex);
    
    return result;
}


char* kedr_target_detector_get_target_name(void)
{
    char* name;

    mutex_lock(&target_module_mutex);
    if(target_name != NULL)
    {
        name = kstrdup(target_name, GFP_KERNEL);
        if(name == NULL)
        {
            pr_err("Failed to allocate name of the target module.");
            name = ERR_PTR(-ENOMEM);
        }
    }
    else
    {
        name = NULL;
    }
    mutex_unlock(&target_module_mutex);
    
    return name;
}

int kedr_target_detector_is_target_loaded(void)
{
    int result;

    mutex_lock(&target_module_mutex);
    result = (target_module != NULL);
    mutex_unlock(&target_module_mutex);
    
    return result;
}