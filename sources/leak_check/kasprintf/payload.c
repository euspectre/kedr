/* A plugin to LeakCheck that tracks the calls to kasprintf() and 
 * kvasprintf(). */

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

#include <stdarg.h>

#include <kedr/core/kedr.h>
#include <kedr/core/kedr_functions_support.h>
#include <kedr/leak_check/leak_check.h>

#include "func_def.h"  /* KEDR_FUNC_USED_* */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");

/* This plugin is created in a different way compared to other payload 
 * modules for KEDR because kasprintf() has a variable argument list. 
 * The standard mechanism of building payload modules does not support this 
 * yet (it would interfere with the handler call chains).
 *
 * As a temporary workaround, processing of kasprintf() and kvasprintf() is
 * enabled in LeakCheck only and no other payload module is allowed to 
 * process these functions at the same time.
 *
 * These limitations may be removed in the future versions of KEDR. */
/* ====================================================================== */

/* For now, only one target module can be processed at a time by this
 * payload. Support for tracking several targets at the same time can be
 * added in the future. */
static struct module *target_module = NULL;
/* ====================================================================== */

#if defined KEDR_FUNC_USED_kasprintf
/* Intermediate replacement function for kasprintf */
static struct kedr_intermediate_info kedr_intermediate_info_kasprintf;
static void * 
kedr_intermediate_func_kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *buf;
	
	/* This is actually part of the implementation of kasprintf */
	va_start(ap, fmt);
	buf = kvasprintf(gfp, fmt, ap);
	va_end(ap);
	
	if (buf != NULL)
		kedr_lc_handle_alloc(target_module, (void *)buf, 
			strlen(buf) + 1, __builtin_return_address(0)); 
	return buf;
}
#endif /* defined KEDR_FUNC_USED_kasprintf */

#if defined KEDR_FUNC_USED_kvasprintf
/* Intermediate replacement function for kvasprintf */
static struct kedr_intermediate_info kedr_intermediate_info_kvasprintf;
static void * 
kedr_intermediate_func_kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	char *buf;
	buf = kvasprintf(gfp, fmt, ap);
	if (buf != NULL)
		kedr_lc_handle_alloc(target_module, (void *)buf, 
			strlen(buf) + 1, __builtin_return_address(0)); 
	return buf;
}
#endif /* defined KEDR_FUNC_USED_kvasprintf */

static struct kedr_intermediate_impl intermediate_impl[] =
{
#if defined KEDR_FUNC_USED_kasprintf
	{
		.orig = (void *)kasprintf,
		.intermediate = (void *)kedr_intermediate_func_kasprintf,
		.info = &kedr_intermediate_info_kasprintf
	},
#endif
#if defined KEDR_FUNC_USED_kvasprintf
	{
		.orig = (void *)kvasprintf,
		.intermediate = (void *)kedr_intermediate_func_kvasprintf,
		.info = &kedr_intermediate_info_kvasprintf
	},
#endif
	/* The terminating element */
	{
		.orig = NULL,
	}
};

static struct kedr_functions_support functions_support =
{
	.intermediate_impl = intermediate_impl
};

int
functions_support_register(void)
{
	return kedr_functions_support_register(&functions_support);
}

void
functions_support_unregister(void)
{
	kedr_functions_support_unregister(&functions_support);
}
/* ====================================================================== */

static void
on_target_load(struct module *m)
{
	target_module = m;
}

static void
on_target_unload(struct module *m)
{
	/* just in case; may help debugging */
	WARN_ON(target_module != m);
	target_module = NULL;
}

/* The fake replacement functions, they must be present in kedr_payload 
 * structure to indicate that kasprintf() and kvasprintf() are to be
 * intercepted. Everything is done in the intermediate replacement 
 * functions rather than here. */
#if defined KEDR_FUNC_USED_kasprintf
static char *
repl_kasprintf(void /* the arguments do not matter */)
{
	return NULL;
}
#endif

#if defined KEDR_FUNC_USED_kvasprintf
static char *
repl_kvasprintf(void /* the arguments do not matter */)
{
	return NULL;
}
#endif

static struct kedr_replace_pair replace_pairs[] = {
#if defined KEDR_FUNC_USED_kasprintf
	{(void *)&kasprintf, (void *)&repl_kasprintf},
#endif
#if defined KEDR_FUNC_USED_kvasprintf
	{(void *)&kvasprintf, (void *)&repl_kvasprintf},
#endif
	{NULL,}
};

static struct kedr_payload payload = {
	.mod                    = THIS_MODULE,

	.pre_pairs              = NULL,
	.post_pairs             = NULL,
	.replace_pairs          = replace_pairs,

	.target_load_callback   = on_target_load,
	.target_unload_callback = on_target_unload
};
/* ====================================================================== */

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void __exit
kedr_lc_kasprintf_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
	functions_support_unregister();

	KEDR_MSG("[kedr_lc_kasprintf] Cleanup complete\n");
}

static int __init
kedr_lc_kasprintf_init_module(void)
{
	int ret = 0;
	KEDR_MSG("[kedr_lc_kasprintf] Initializing\n");
	
	ret = functions_support_register();
	if(ret != 0)
		goto fail_supp;

	ret = kedr_payload_register(&payload);
	if (ret != 0) 
		goto fail_reg;
  
	return 0;

fail_reg:
	functions_support_unregister();
fail_supp:
	return ret;
}

module_init(kedr_lc_kasprintf_init_module);
module_exit(kedr_lc_kasprintf_cleanup_module);
/* ====================================================================== */
