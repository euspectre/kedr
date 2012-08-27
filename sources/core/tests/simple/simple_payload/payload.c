/*
 * A simple payload module - for testing purposes only.
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
 
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/*********************************************************************/

#include <kedr/core/kedr.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/errno.h>    /* error codes */

/*********************************************************************
 * Replacement functions
 *********************************************************************/
static void*
repl___kmalloc(size_t size, gfp_t flags)
{
    void* ret_val;
    
    /* Call the target function */
    ret_val = __kmalloc(size, flags);
    /* Do nothing more */

    return ret_val;
}

static void
repl_kfree(void* p)
{
    /* Call the target function */
    kfree(p);
    return;
}
/*********************************************************************/

/* Names and addresses of the functions of interest */
static struct kedr_replace_pair replace_pairs[] =
{
    {
        .orig 		= (void*)&__kmalloc,
        .replace 	= (void*)repl___kmalloc
    },
    {
        .orig 		= (void*)&kfree,
        .replace 	= (void*)repl_kfree
	},
	{
        .orig = NULL
	}
};

static struct kedr_payload payload = {
    .mod                    = THIS_MODULE,

    .replace_pairs          = replace_pairs,

    .target_load_callback   = NULL,
    .target_unload_callback = NULL
};
/*********************************************************************/

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void
simple_payload_cleanup_module(void)
{
    kedr_payload_unregister(&payload);
    functions_support_unregister();
}

static int __init
simple_payload_init_module(void)
{
    int result;
    
    result = functions_support_register();
    if(result) return result;
    
    result = kedr_payload_register(&payload);
    if(result)
    {
        functions_support_unregister();
        return result;
    }
    
    return 0;
}

module_init(simple_payload_init_module);
module_exit(simple_payload_cleanup_module);
/*********************************************************************/
