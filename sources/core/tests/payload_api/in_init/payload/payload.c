/*********************************************************************
 * This module contains a replacement function for __kmalloc that checks
 * whether the latter is called during initialization of the target module 
 * or not. 
 * The result is made available to user space via "target_in_init"
 * parameter.
 *********************************************************************/
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
#include <linux/init.h>

#include <linux/slab.h>     /* __kmalloc() */

#include <kedr/core/kedr.h>

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
repl___kmalloc(size_t size, gfp_t flags,
	struct kedr_function_call_info* call_info)
{
	void* ret_val;
    target_in_init = (kedr_target_module_in_init() ? 1 : 0);
	ret_val = __kmalloc(size, flags);
	return ret_val;
}
/*********************************************************************/

static struct kedr_replace_pair replace_pairs[] =
{
	{
		.orig = (void*)&__kmalloc,
		.replace = (void*)&repl___kmalloc
	},
	{
		.orig = NULL
	}
};

static struct kedr_payload payload = {
	.mod            = THIS_MODULE,

	.replace_pairs	= replace_pairs
};
/*********************************************************************/

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void
kedr_test_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
	functions_support_unregister();
	return;
}

static int __init
kedr_test_init_module(void)
{
	int result = functions_support_register();
	if(result) return result;
	
	result = kedr_payload_register(&payload);
	if(result)
	{
		functions_support_unregister();
		return result;
	}
	
	return 0;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
