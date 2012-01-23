/*
 * Module envelope around kedr_base with registering payload.
 * Also try to use payload.
 */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
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

#include "kedr_base_internal.h"

#include <kedr/core/kedr.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

#define func1 (void*)0x1234
#define repl1 (void*)0x654
#define pre1 (void*)0x6554

static struct kedr_replace_pair replace_pairs[] =
{
    {
        .orig = func1,
        .replace = repl1
    },
    {
        .orig = NULL
    }
};

static struct kedr_pre_pair pre_pairs[] =
{
    {
        .orig = func1,
        .pre = pre1
    },
    {
        .orig = NULL
    }
};

static struct kedr_payload payload =
{
    //.mod = THIS_MODULE,
    
    .replace_pairs = replace_pairs,
    .pre_pairs = pre_pairs,
};

static int __init
kedr_module_init(void)
{
    int result;
    const struct kedr_base_interception_info* info;
    
    result = kedr_base_init(NULL);
    if(result) return result;

    result = kedr_payload_register(&payload);
    if(result) goto err_payload;
    
    info = kedr_base_target_load_callback(THIS_MODULE);
    if((info == NULL) || IS_ERR(info))
    {
        result = -EINVAL;
        goto err_target_load;
    }
    if(info[0].orig == NULL)
    {
        pr_err("Emptry interception info array");
        result = -EINVAL;
        goto err_interception_info;
    }
    if(info[1].orig != NULL)
    {
        pr_err("Interception info array should contain 1 element, but it contains more.");
        result = -EINVAL;
        goto err_interception_info;
    }

    if(info[0].orig != func1)
    {
        pr_err("Interception info array is not contain 'func1', but should.");
        result = -EINVAL;
        goto err_interception_info;
    }

    if((info[0].post != NULL) && (info[0].post[0] != NULL))
    {
        pr_err("Array of post- functions should be empty, but it is not.");
        result = -EINVAL;
        goto err_interception_info;
    }

    if((info[0].pre == NULL) || (info[0].pre[0] == NULL))
    {
        pr_err("Array of pre- functions shouldn't be empty, but it is.");
        result = -EINVAL;
        goto err_interception_info;
    }
    
    if(info[0].pre[1] != NULL)
    {
        pr_err("Array of pre- functions should contain only one element, but it contains more.");
        result = -EINVAL;
        goto err_interception_info;
    }

    if(info[0].pre[0] != pre1)
    {
        pr_err("Array of pre- functions should contain 'pre1', but it isn't.");
        result = -EINVAL;
        goto err_interception_info;
    }

    if(info[0].replace != repl1)
    {
        pr_err("Replace function should be 'repl1', but it isn't.");
        result = -EINVAL;
        goto err_interception_info;
    }
    
    return 0;

err_interception_info:
    kedr_base_target_unload_callback(THIS_MODULE);
err_target_load:
    kedr_payload_unregister(&payload);
err_payload:
    kedr_base_destroy();
    
    return result;
}

static void __exit
kedr_module_exit(void)
{
    kedr_base_target_unload_callback(THIS_MODULE);
    kedr_payload_unregister(&payload);
    kedr_base_destroy();
}

module_init(kedr_module_init);
module_exit(kedr_module_exit);