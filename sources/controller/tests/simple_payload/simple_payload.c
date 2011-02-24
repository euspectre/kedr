/*
 * A simple payload module - for testing purposes only.
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
 
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/*********************************************************************/

#include <kedr/base/common.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/errno.h>    /* error codes */

/*********************************************************************
 * Replacement functions
 *********************************************************************/
static void*
repl___kmalloc(size_t size, gfp_t flags)
{
    void* returnValue;
    
    /* Call the target function */
    returnValue = __kmalloc(size, flags);
    /* Do nothing more */

    return returnValue;
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
static void* orig_addrs[] = {
    (void*)&__kmalloc,
    (void*)&kfree
};

/* Addresses of the replacement functions */
static void* repl_addrs[] = {
    (void*)&repl___kmalloc,
    (void*)&repl_kfree
};

static struct kedr_payload payload = {
    .mod                    = THIS_MODULE,
    .repl_table.orig_addrs  = &orig_addrs[0],
    .repl_table.repl_addrs  = &repl_addrs[0],
    .repl_table.num_addrs   = ARRAY_SIZE(orig_addrs),
    .target_load_callback   = NULL,
    .target_unload_callback = NULL
};
/*********************************************************************/

static void
simple_payload_cleanup_module(void)
{
    kedr_payload_unregister(&payload);
    return;
}

static int __init
simple_payload_init_module(void)
{
    BUG_ON( ARRAY_SIZE(orig_addrs) != 
        ARRAY_SIZE(repl_addrs));
    
    return kedr_payload_register(&payload);
}

module_init(simple_payload_init_module);
module_exit(simple_payload_cleanup_module);
/*********************************************************************/
