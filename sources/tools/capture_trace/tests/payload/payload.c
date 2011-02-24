/*********************************************************************
 * Module: kedr_payload
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

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <kedr/base/common.h>
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

void repl_kfree(void* p)
{
    trace_kedr_payload(2);
    kfree(p);
}
static void* orig_addrs[] = {(void*)kfree};
static void* repl_addrs[] = {(void*)repl_kfree};

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
	.repl_table.orig_addrs 	= &orig_addrs[0],
	.repl_table.repl_addrs 	= &repl_addrs[0],
	.repl_table.num_addrs	= sizeof(orig_addrs) / sizeof(orig_addrs[0]),
    .target_load_callback   = target_load_callback,
    .target_unload_callback = target_unload_callback
};

/* ================================================================ */
static void
kedr_payload_cleanup_module(void)
{
    kedr_payload_unregister(&payload);
	return;
}

static int __init
kedr_payload_init_module(void)
{
    return kedr_payload_register(&payload);
}

module_init(kedr_payload_init_module);
module_exit(kedr_payload_cleanup_module);
/* ================================================================ */


