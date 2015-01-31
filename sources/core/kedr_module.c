/*
 * Combine different KEDR components into one module.
 */

/* ========================================================================
 * Copyright (C) 2012-2014, KEDR development team
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

#include <kedr/core/kedr.h>
#include <kedr/core/kedr_functions_support.h>

#include "kedr_base_internal.h"
#include "kedr_instrumentor_internal.h"
#include "kedr_functions_support_internal.h"
#include "kedr_target_detector_internal.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/slab.h> /* kmalloc */
#include <linux/string.h> /* kstrdup, strlen, strcpy */

#include <linux/mutex.h>

#include "kedr_internal.h"
#include "config.h"

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

#define COMPONENT_STRING "main: "

/* 
 * Number of loaded target modules.
 */
int n_targets_loaded = 0;

/* 
 * Module parameter with name of the target module.
 * It can be passed to 'insmod' as an argument, for example,
 *  insmod kedr.ko target_name="module_to_be_analyzed".
 * 
 * This module parameter is implemented by hand
 * (with 'set' and 'get' callbacks).
 */

// Parameter value which means that target is not set.
#define target_name_not_set "none"

static int
target_name_param_get(char* buffer,
#if defined(MODULE_PARAM_CREATE_USE_OPS_STRUCT)
    const struct kernel_param *kp
#elif defined(MODULE_PARAM_CREATE_USE_OPS)
    struct kernel_param *kp
#else 
#error Unknown way to create module parameter with callbacks
#endif
)
{
    // 'buffer' is of 4K size.
    int result = kedr_target_detector_get_target_name(buffer, 4096);
    if(result == 0)
    {
        result = strlen(target_name_not_set);
        memcpy(buffer, target_name_not_set, result);
    }
    
    return result;
}

static int
target_name_param_set(const char* val,
#if defined(MODULE_PARAM_CREATE_USE_OPS_STRUCT)
    const struct kernel_param *kp
#elif defined(MODULE_PARAM_CREATE_USE_OPS)
    struct kernel_param *kp
#else 
#error Unknown way to create module parameter with callbacks
#endif
)
{
    int result = 0;
    
    // Check special value, which means "clear targets".
    if(!strcmp(val, target_name_not_set))
    {
        result = kedr_target_detector_set_target_name("");
    }
    else
    {
        result = kedr_target_detector_set_target_name(val);
    }
    
    return result;
}

#if defined(MODULE_PARAM_CREATE_USE_OPS_STRUCT)
static struct kernel_param_ops target_name_param_ops =
{
    .set = target_name_param_set,
    .get = target_name_param_get,
};
module_param_cb(target_name,
    &target_name_param_ops,
    NULL,
    S_IRUGO | S_IWUSR);
#elif defined(MODULE_PARAM_CREATE_USE_OPS)
module_param_call(target_name,
    target_name_param_set, target_name_param_get,
    NULL,
    S_IRUGO | S_IWUSR);
#else 
#error Unknown way to create module parameter with callbacks
#endif


/********************************************************************/
/* Replace pairs for current session. */
static const struct kedr_instrumentor_replace_pair* replace_pairs;

// Called when target module is loaded.
int
on_target_load(struct module* m)
{
    int result;
    
    if(!n_targets_loaded)
    {
        // Start new session
        const struct kedr_base_interception_info* interception_info =
            kedr_base_session_start();
        
        if(IS_ERR(interception_info))
        {
            return PTR_ERR(interception_info);
        }
        
        replace_pairs = kedr_functions_support_prepare(interception_info);
        
        if(IS_ERR(replace_pairs))
        {
            kedr_base_session_stop();
            return PTR_ERR(replace_pairs);
        }
    }
    
    result = kedr_instrumentor_replace_functions(m, replace_pairs);
    if(result)
    {
        if(!n_targets_loaded)
        {
            kedr_functions_support_release();
            kedr_base_session_stop();
        }
        return result;
    }
    
    kedr_base_target_load(m);
    
    n_targets_loaded++;
    
    return 0;
}

// Called when target module is unloaded.
void
on_target_unload(struct module* m)
{
    kedr_base_target_unload(m);
    kedr_instrumentor_replace_clean(m);
    n_targets_loaded--;
    
    if(!n_targets_loaded)
    {
        kedr_functions_support_release();
        kedr_base_session_stop();
    }
}


static int __init
kedr_module_init(void)
{
    int result;

    result = kedr_functions_support_init();
    if (result) goto functions_support_err;
    
    result = kedr_instrumentor_init();
    if (result) goto instrumentor_err;
    
    result = kedr_base_init();
    if (result) goto base_err;
    
    result = kedr_target_detector_init();
    if (result) goto target_detector_err;
    
    return 0;

target_detector_err:
    kedr_base_destroy();
base_err:
    kedr_instrumentor_destroy();
instrumentor_err:
    kedr_functions_support_destroy();
functions_support_err:
    return result;
}

static void __exit
kedr_module_exit(void)
{
    kedr_target_detector_destroy();
    kedr_base_destroy();
    kedr_instrumentor_destroy();
    kedr_functions_support_destroy();
}

module_init(kedr_module_init);
module_exit(kedr_module_exit);

// The only functions exported from the KEDR module.
EXPORT_SYMBOL(kedr_payload_register);
EXPORT_SYMBOL(kedr_payload_unregister);

EXPORT_SYMBOL(kedr_functions_support_register);
EXPORT_SYMBOL(kedr_functions_support_unregister);

EXPORT_SYMBOL(kedr_target_module_in_init);
