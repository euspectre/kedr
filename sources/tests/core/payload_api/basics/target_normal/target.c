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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");

/* ================================================================ */
void *p = NULL;

static void
kedr_test_cleanup_module(void)
{
	kfree(p);
	return;
}

static int __init
kedr_test_init_module(void)
{
	p = kmalloc(123, GFP_KERNEL);
	if(!p) return -ENOMEM;
	
	return 0; /* success */
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/* ================================================================ */
