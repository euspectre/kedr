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

#include "kedr_functions_support_internal.h"
#include <kedr/core/kedr.h>
#include <kedr/core/kedr_functions_support.h>

#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

#define test_orig (void*)1001
#define test_intermediate1 (void*)2001
#define test_intermediate2 (void*)2002


static struct kedr_intermediate_info intermediate_info1;
static struct kedr_intermediate_impl impls1[] =
{
    {
        .orig = test_orig,
        .intermediate = test_intermediate1,
        .info = &intermediate_info1,
    },
    {
        .orig = NULL
    }
};


static struct kedr_functions_support support1 =
{
    .mod = THIS_MODULE,
    
    .intermediate_impl = impls1
};

static struct kedr_intermediate_info intermediate_info2;
static struct kedr_intermediate_impl impls2[] =
{
    {
        .orig = test_orig,
        .intermediate = test_intermediate2,
        .info = &intermediate_info2,
    },
    {
        .orig = NULL
    }
};


static struct kedr_functions_support support2 =
{
    .mod = THIS_MODULE,
    
    .intermediate_impl = impls2
};

static void* pre_functions[] =
{
    (void*)0x10001,
    NULL
};

static void* post_functions[] =
{
    (void*)0x11001,
    NULL
};

static struct kedr_base_interception_info interception_info[] =
{
    {
        .orig = test_orig,
        .pre = pre_functions,
        .post = post_functions,
        .replace = (void*)0x12001,
    },
    {
        .orig = NULL
    }
};

static int test_supports_register_and_use(void)
{
    int result;
    const struct kedr_instrumentor_replace_pair* replace_pairs;
    const struct kedr_intermediate_info* intermediate_info_p;
    
    result = kedr_functions_support_register(&support1);
    if(result)
    {
        pr_err("Failed to register first functions support.");
        goto err_register1;
    }
    result = kedr_functions_support_register(&support2);
    if(result)
    {
        pr_err("Failed to register second functions support.");
        goto err_register2;
    }

    
    result = kedr_functions_support_function_use(test_orig);
    if(result)
    {
        pr_err("Failed to mark supported function as used.");
        goto err_function_use;
    }
    
    replace_pairs = kedr_functions_support_prepare(interception_info);
    if(IS_ERR(replace_pairs))
    {
        pr_err("Failed to prepare functions support.");
        result = PTR_ERR(replace_pairs);
        goto err_prepare;
    }

    if(replace_pairs == NULL)
    {
        pr_err("kedr_functions_support_prepare() returns NULL.");
        goto err_replace;
    }
    
    if(replace_pairs[0].orig == NULL)
    {
        pr_err("kedr_functions_support_prepare() returns empty array, but shouldn't.");
        goto err_replace;
    }
    
    if(replace_pairs[1].orig != NULL)
    {
        pr_err("Replace array should contain one element, but it contains more.");
        goto err_replace;
    }

    if(replace_pairs[0].orig != test_orig)
    {
        pr_err("Replace array should contain functions %p, but it contains %p.",
            test_orig, replace_pairs[0].orig);
        goto err_replace;
    }

    if(replace_pairs[0].repl == test_intermediate1)
    {
        intermediate_info_p = &intermediate_info1;
    }
    else if(replace_pairs[0].repl == test_intermediate2)
    {
        intermediate_info_p = &intermediate_info2;
    }
    else
    {
        pr_err("Replace array should contain replacement %p or %p for function %p, "
            "but it contains %p.",
            test_intermediate1, test_intermediate2,
            test_orig, replace_pairs[0].repl);
        goto err_replace;
    }
    
    if(intermediate_info_p->post != interception_info[0].post)
    {
        pr_err("Array of post- functions should be set to %p, but it is %p.",
            interception_info[0].post, intermediate_info_p->post);
        goto err_replace;
    }
    
    if(intermediate_info_p->pre != interception_info[0].pre)
    {
        pr_err("Array of pre- functions should be set to %p, but it is %p.",
            interception_info[0].pre, intermediate_info_p->pre);
        goto err_replace;
    }

    if(intermediate_info_p->replace != interception_info[0].replace)
    {
        pr_err("Replace function should be set to %p, but it is %p.",
            interception_info[0].replace, intermediate_info_p->replace);
        goto err_replace;
    }

    kedr_functions_support_release();
    kedr_functions_support_function_unuse(test_orig);
    kedr_functions_support_unregister(&support2);
    kedr_functions_support_unregister(&support1);
    
    return 0;

err_replace:
    result = -EINVAL;
    kedr_functions_support_release();
err_prepare:
    kedr_functions_support_function_unuse(test_orig);
err_function_use:
    kedr_functions_support_unregister(&support2);
err_register2:
    kedr_functions_support_unregister(&support1);
err_register1:
    return result;
}

static int __init
functions_support_module_init(void)
{
    int result;
    
    result = kedr_functions_support_init();
    if(result)
    {
        pr_err("Failed to initialize functions support subsystem.");
        return result;
    }
    
    result = test_supports_register_and_use();
    
    kedr_functions_support_destroy();
    
    return result;
}

static void __exit
functions_support_module_exit(void)
{
}

module_init(functions_support_module_init);
module_exit(functions_support_module_exit);