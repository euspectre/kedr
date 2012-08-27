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

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/slab.h>

#include <kedr/core/kedr.h>

/* ================================================================ */
MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");
/* ================================================================ */

unsigned long __kmalloc_caller_address = 0;
module_param_named(__kmalloc, __kmalloc_caller_address, ulong, S_IRUGO);

/* ================================================================ */
/* Interception functions */
static void
post___kmalloc(size_t size, gfp_t flags, void* ret_val,
    struct kedr_function_call_info* call_info)
{
    __kmalloc_caller_address = (unsigned long)call_info->return_address;
}

/* ================================================================ */

/* Names and addresses of the functions of interest */

static struct kedr_post_pair post_pairs[] =
{
    {
        .orig = (void*)&__kmalloc,
        .post  = (void*)&post___kmalloc
    },
    {
        .orig = NULL
    }
};

static struct kedr_payload get_caller_address_payload = {
    .mod                    = THIS_MODULE,

    .post_pairs = post_pairs,
    
    .target_load_callback   = NULL,
    .target_unload_callback = NULL
};

/* ================================================================ */

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static int __init
get_caller_address_init(void)
{
    int ret = 0;
    
    ret = functions_support_register();
    if(ret)
    {
        printk(KERN_ERR "[get_caller_address] failed to register functions support for payload.\n");
        return ret;
    }
    
    ret = kedr_payload_register(&get_caller_address_payload);
    if (ret < 0)
    {
        printk(KERN_ERR "[get_caller_address] failed to register payload module.\n");
        functions_support_unregister();
        return ret;
    }
    return 0;
}

static void
get_caller_address_exit(void)
{
    kedr_payload_unregister(&get_caller_address_payload);
    functions_support_unregister();
    return;
}

module_init(get_caller_address_init);
module_exit(get_caller_address_exit);
