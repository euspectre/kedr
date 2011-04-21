/*
 * Trigger on_target_load event of the kedr_base and verify result.
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

#include "kedr_base_internal.h"
#include <kedr/core/kedr.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

/* Determine, which result of kedr_base_target_load_callback() is expected. */
int situation = 0;
module_param(situation, int, S_IRUGO);

/* Print interception info. */
static void print_interception_info(const struct kedr_base_interception_info* info)
{
    const struct kedr_base_interception_info* info_elem;
    for(info_elem = info; info_elem->orig != NULL; info_elem++)
    {
        pr_info("\tFunction %p:", info_elem->orig);
        if(info_elem->pre != NULL && info_elem->pre[0] != NULL)
        {
            void** pre;
            pr_info("\t\tPre- functions:");
            for(pre = info_elem->pre; *pre != NULL; pre++)
            {
                pr_info("\t\t\t%p", *pre);
            }
        }
        if(info_elem->post != NULL && info_elem->post[0] != NULL)
        {
            void** post;
            pr_info("\t\tPost- functions:");
            for(post = info_elem->post; *post != NULL; post++)
            {
                pr_info("\t\t\t%p", *post);
            }
        }
        if(info_elem->replace != NULL)
        {
            pr_info("\t\tReplace function: %p", info_elem->replace);
        }
    }
}

/* Verify that interception info element corresponds to the expected one. */
static int
verify_interception_info_elem(
    const struct kedr_base_interception_info* elem,
    const struct kedr_base_interception_info* elem_expected
)
{
    if((elem->pre != NULL) && (elem->pre[0] != NULL))
    {
        int n_pre = 0, n_pre_expected = 0;
        void **pre, **pre_expected;
        if((elem_expected->pre == NULL) || (elem_expected->pre[0] == NULL))
        {
            pr_err("Interception information for function %p should't contain any pre-function, "
                "but it does.", elem->orig);
            goto err;
        }
        for(pre = elem->pre; *pre != NULL; pre++)
            n_pre++;
        
        for(pre_expected = elem_expected->pre; *pre_expected != NULL; pre_expected++)
            n_pre_expected++;
        
        if(n_pre != n_pre_expected)
        {
            pr_err("Interception information for function %p contains %d pre functions, "
                "but should contain %d.", elem->orig, n_pre, n_pre_expected);
        }
        for(pre_expected = elem_expected->pre; *pre_expected != NULL; pre_expected++)
        {
            for(pre = elem->pre; *pre != NULL; pre++)
            {
                if(*pre == *pre_expected) break;
            }
            if(*pre == NULL)
            {
                pr_err("Interception information for function %p doesn't contain pre-function %p.",
                    elem->orig, *pre_expected);
                goto err;
            }
        }
    }
    else if((elem_expected->pre != NULL) && (elem_expected->pre[0] != NULL))
    {
        pr_err("Interception information for function %p doesn't contain any pre-function, "
                "but it should.", elem->orig);
        goto err;
    }
    
    if(elem->replace != elem_expected->replace)
    {
        if(elem->replace != NULL)
        {
            if(elem_expected->replace == NULL)
            {
                pr_err("Interception information for function %p contains replace function, "
                    "but it shouldn't.", elem->orig);
            }
            else
            {
                pr_err("Interception information for function %p should contain replace function %p, "
                    "but it contains %p.", elem->orig, elem_expected->replace, elem->replace);
            }
        }
        else
        {
            pr_err("Interception information for function %p should contain replace function, "
                "but it doesn't.", elem->orig);
        }
        goto err;
    }
    
    if((elem->post != NULL) && (elem->post[0] != NULL))
    {
        int n_post = 0, n_post_expected = 0;
        void **post, **post_expected;
        if((elem_expected->post == NULL) || (elem_expected->post[0] == NULL))
        {
            pr_err("Interception information for function %p should't contain any post-function, "
                "but it does.", elem->orig);
            goto err;
        }
        for(post = elem->post; *post != NULL; post++)
            n_post++;
        
        for(post_expected = elem_expected->post; *post_expected != NULL; post_expected++)
            n_post_expected++;
        
        if(n_post != n_post_expected)
        {
            pr_err("Interception information for function %p contains %d post functions, "
                "but should contain %d.", elem->orig, n_post, n_post_expected);
        }
        for(post_expected = elem_expected->post; *post_expected != NULL; post_expected++)
        {
            for(post = elem->post; *post != NULL; post++)
            {
                if(*post == *post_expected) break;
            }
            if(*post == NULL)
            {
                pr_err("Interception information for function %p doesn't contain post-function %p.",
                    elem->orig, *post_expected);
                goto err;
            }
        }
    }
    else if((elem_expected->post != NULL) && (elem_expected->post[0] != NULL))
    {
        pr_err("Interception information for function %p doesn't contain any post-function, "
                "but it should.", elem->orig);
        goto err;
    }
    
    return 0;
err:
    return -EINVAL;
}

/*
 * Verify, that content of the interception info array corresponds
 * to the expected one.
 * 
 * If info array is correct, return 0.
 * 
 * On error, return negative error code.
 */
static int
verify_interception_info(const struct kedr_base_interception_info* info_array,
    const struct kedr_base_interception_info* info_array_expected)
{
    const struct kedr_base_interception_info *info_elem, *info_elem_expected;
    
    int n_array = 0, n_array_expected = 0;
    
    for(info_elem = info_array; info_elem->orig != NULL; info_elem++)
        n_array++;

    for(info_elem_expected = info_array_expected;
        info_elem_expected->orig != NULL;
        info_elem_expected++
    )
        n_array_expected++;

    if(n_array != n_array_expected)
    {
        pr_err("Interception information should contain %d functions, "
            "but it contains %d.", n_array_expected, n_array);
    }

    for(info_elem_expected = info_array_expected;
        info_elem_expected->orig != NULL;
        info_elem_expected++
    )
    {
        for(info_elem = info_array;
            info_elem->orig != NULL;
            info_elem++)
        {
            if(info_elem->orig == info_elem_expected->orig) break;
        }
        if(info_elem->orig == NULL)
        {
            pr_err("Interception information should contain function %p, "
                "but it doesn't.", info_elem_expected->orig);
            goto err;
        }
        if(verify_interception_info_elem(info_elem, info_elem_expected))
        {
            pr_err("Incorrect interception information for function %p.",
                info_elem->orig);
            goto err;
        }
    }

    return 0;

err:
    pr_info("Interception information is:");
    print_interception_info(info_array);
    pr_info("*content end*");
    
    return -EINVAL;
}

/* 
 * Verification routines for concrete situations.
 */

/* Situation_12 - 1st and 2nd payloads are loaded.*/

static void* info_12_1_pre[] =
{
    (void*)0x2001,
    NULL
};

static void* info_12_2_pre[] =
{
    (void*)0x3001,
    NULL
};

static void* info_12_2_post[] =
{
    (void*)0x2002,
    NULL
};

static void* info_12_3_post[] =
{
    (void*)0x3002,
    NULL
};

static struct kedr_base_interception_info info_12[] =
{
    {
        .orig = (void*)0x1001,
        .pre = info_12_1_pre,
    },
    {
        .orig = (void*)0x1002,
        .pre = info_12_2_pre,
        .post = info_12_2_post,
    },
    {
        .orig = (void*)0x1003,
        .post = info_12_3_post,
    },
    {
        .orig = NULL
    }
};


static int check_12(const struct kedr_base_interception_info* info)
{
    return verify_interception_info(info, info_12);
}

/* Situation_13 - 1st and 3rd payloads are loaded.*/

static void* info_13_1_pre[] =
{
    (void*)0x2001,
    (void*)0x4001,
    (void*)0x4002,
    NULL
};

//static const void* info_13_1_replace = (void*)0x5002;

static void* info_13_2_post[] =
{
    (void*)0x2002,
    NULL
};

static void* info_13_3_pre[] =
{
    (void*)0x4003,
    NULL
};

static struct kedr_base_interception_info info_13[] =
{
    {
        .orig = (void*)0x1001,
        .pre = info_13_1_pre,
        .replace = (void*)0x5002,
    },
    {
        .orig = (void*)0x1002,
        .post = info_13_2_post,
    },
    {
        .orig = (void*)0x1003,
        .pre = info_13_3_pre,
    },
    {
        .orig = NULL
    }
};


static int check_13(const struct kedr_base_interception_info* info)
{
    return verify_interception_info(info, info_13);
}


enum situations
{
    situation_invalid   = -1,
    situation_12     = 1,
    situation_13     = 2,
};

static int __init
verificator_init(void)
{
    int result;
    const struct kedr_base_interception_info* info =
        kedr_base_target_load_callback(THIS_MODULE);
    if(IS_ERR(info))
    {
        pr_err("kedr_base_target_load_callback() failed.");
        return PTR_ERR(info);
    }
    switch((enum situations)situation)
    {
        case situation_12:
            result = check_12(info);
        break;
        case situation_13:
            result = check_13(info);
        break;

        case situation_invalid:
            pr_err("'situation' parameter should be set");
            result = -EINVAL;
        break;
        default:
            pr_err("'situation' parameter should be set to the correct value");
            result = -EINVAL;
        break;
    }

    kedr_base_target_unload_callback(THIS_MODULE);
    return result;
}

static void __exit
verificator_exit(void)
{
}

module_init(verificator_init);
module_exit(verificator_exit);