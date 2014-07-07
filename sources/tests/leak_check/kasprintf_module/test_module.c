/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Copyright (C) 2011, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <stdarg.h>
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
/* ====================================================================== */

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");
/* ====================================================================== */

static void
trigger_kasprintf(void)
{
	char *bytes = NULL;
	char *buf = NULL;
	const char *str = "-16179-";
	
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		strcpy(&bytes[0], str);
		buf = kasprintf(GFP_KERNEL, "@%s@", bytes);
	}
	kfree(bytes);
	kfree(buf);
}

/* This is actually the code of kasprintf(). */
static char * 
trigger_kvasprintf_wrapper(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return p;
}

static void
trigger_kvasprintf(void)
{
	char *bytes = NULL;
	char *buf = NULL;
	const char *str = "-16179-";
	
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		strcpy(&bytes[0], str);
		buf = trigger_kvasprintf_wrapper(GFP_KERNEL, "@%s@", 
			bytes);
	}
	kfree(bytes);
	kfree(buf);	
}

static void
kedr_test_cleanup_module(void)
{
	trigger_kasprintf();
	trigger_kvasprintf();
}

static int __init
kedr_test_init_module(void)
{
	return 0;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/* ====================================================================== */
