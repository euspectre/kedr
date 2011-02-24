/*********************************************************************
 * This module contains a replacement function for __kmalloc that checks
 * whether the latter is called during initialization of the target module 
 * or not. 
 * The result is made available to user space via "target_in_init"
 * parameter.
 *********************************************************************/
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
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/slab.h>     /* __kmalloc() */

#include <kedr/base/common.h>

/*********************************************************************/
MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");
/*********************************************************************/

/* "target_in_init" parameter.
 * 0 - target functon was called from module core the last time
 * 1 - target functon was called from module init the last time
 * 2 - target function was not called at all
 */
unsigned long target_in_init = 2;
module_param(target_in_init, ulong, S_IRUGO);

/*********************************************************************
 * Replacement functions
 *********************************************************************/
static void*
repl___kmalloc(size_t size, gfp_t flags)
{
	void* returnValue;
    target_in_init = (kedr_target_module_in_init() ? 1 : 0);
	returnValue = __kmalloc(size, flags);
	return returnValue;
}
/*********************************************************************/

/* Names and addresses of the functions of interest */
static void* orig_addrs[] = {
	(void*)&__kmalloc
};

/* Addresses of the replacement functions */
static void* repl_addrs[] = {
	(void*)&repl___kmalloc
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
kedr_test_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
	return;
}

static int __init
kedr_test_init_module(void)
{
	BUG_ON(	ARRAY_SIZE(orig_addrs) != 
		ARRAY_SIZE(repl_addrs));
	return kedr_payload_register(&payload);
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
