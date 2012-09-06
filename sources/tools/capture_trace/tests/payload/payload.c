/*********************************************************************
 * Module: kedr_payload
 *********************************************************************/

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
 
#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <kedr/core/kedr.h>
#include <kedr/trace/trace.h>

#include <linux/cdev.h>
#include <linux/slab.h>	    /* kmalloc(), kfree() */

/*********************************************************************/

struct kedr_trace_data_kfree
{
	int some_arg;
};

static inline int
kedr_trace_pp_function_kfree(char* dest, size_t size,
	const void* data)
{
	const struct kedr_trace_data_kfree* data_orig =
		(const struct kedr_trace_data_kfree*)data;
	 
	return snprintf(dest, size, "Capture me payload %d",
		data_orig->some_arg);
}

static void trace_kedr_payload(int some_arg)
{
	struct kedr_trace_data_kfree data;
	data.some_arg = some_arg;
	
	kedr_trace(kedr_trace_pp_function_kfree, &data, sizeof(data));
}

static void pre_kfree(void* p, struct kedr_function_call_info* call_info)
{
    trace_kedr_payload(2);
}

static struct kedr_pre_pair pre_pairs[] =
{
	{
		.orig = (void*)&kfree,
		.pre  = (void*)&pre_kfree,
	},
	{
		.orig = NULL
	}
};

static void
target_load_callback(struct module *target_module)
{
	kedr_trace_marker_target(target_module, THIS_MODULE, 1);
}

static void
target_unload_callback(struct module *target_module)
{
	kedr_trace_marker_target(target_module, THIS_MODULE, 0);
}


static struct kedr_payload payload = {
	.mod 			        = THIS_MODULE,

	.pre_pairs              = pre_pairs,

    .target_load_callback   = target_load_callback,
    .target_unload_callback = target_unload_callback
};

/* ================================================================ */

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void
kedr_payload_cleanup_module(void)
{
    kedr_payload_unregister(&payload);
	functions_support_unregister();

    kedr_trace_pp_unregister();
}

static int __init
kedr_payload_init_module(void)
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

module_init(kedr_payload_init_module);
module_exit(kedr_payload_cleanup_module);
/* ================================================================ */


