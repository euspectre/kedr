/* leak_check.c - implementation of the LeakCheck core */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 *
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
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>

#include <kedr/core/kedr.h>
#include <kedr/leak_check/leak_check.h>
#include <kedr/util/stack_trace.h>

#include "leak_check_impl.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Default number of stack frames to output (at most) */
#define KEDR_STACK_DEPTH_DEFAULT 12

/* At most 'max_stack_entries' stack entries will be output for each 
 * suspicious allocation or deallocation. 
 * Should not exceed KEDR_MAX_FRAMES (see <kedr/util/stack_trace.h>). */
unsigned int stack_depth = KEDR_STACK_DEPTH_DEFAULT;
module_param(stack_depth, uint, S_IRUGO);
/* ====================================================================== */

static void
on_target_load(struct module *m)
{
	// TODO
}

static void
on_target_unload(struct module *m)
{
	// TODO
}

/* [NB] Both the LeakCheck core and the payloads need to be notified when
 * the target has loaded or is about to unload. 
 * The LeakCheck core itself is also a payload module for KEDR, so it will 
 * receive appropriate notifications. */
static struct kedr_payload payload = {
	.mod                    = THIS_MODULE,
	.pre_pairs              = NULL,
	.post_pairs             = NULL,
	.target_load_callback   = on_target_load,
	.target_unload_callback = on_target_unload
};
/* ====================================================================== */

void
kedr_lc_handle_alloc(struct module *mod, const void *block, size_t size, 
	const void *caller_address)
{
	// TODO
}
EXPORT_SYMBOL(kedr_lc_handle_alloc);

void
kedr_lc_handle_free(struct module *mod, const void *block, 
	const void *caller_address)
{
	// TODO
}
EXPORT_SYMBOL(kedr_lc_handle_free);
/* ====================================================================== */

static void __exit
leak_check_cleanup_module(void)
{
	/* Unregister from KEDR core first, then clean up the rest. */
	kedr_payload_unregister(&payload);
	/*klc_output_fini();*/
}

static int __init
leak_check_init_module(void)
{
	int ret = 0;
	
	if (stack_depth == 0 || stack_depth > KEDR_MAX_FRAMES) {
		pr_err("[kedr_leak_check] "
		"Invalid value of 'stack_depth': %u (should be a positive "
		"integer not greater than %u)\n",
			stack_depth,
			KEDR_MAX_FRAMES
		);
		return -EINVAL;
	}
	
	/*klc_init_mbi_storage();
	
	ret = klc_output_init();
	if (ret != 0)
		return ret;*/
	
	// TODO: initialize everything before registering as a payload module

	ret = kedr_payload_register(&payload);
	if (ret != 0) 
		goto fail_reg;
  
	return 0;

fail_reg:
	// cleanup all the stuff initialized before kedr_payload_register()
	//klc_output_fini();
	return ret;
}

module_init(leak_check_init_module);
module_exit(leak_check_cleanup_module);
