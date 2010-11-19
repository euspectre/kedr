/* ========================================================================
 * Copyright (C) 2010, Institute for System Programming 
 *                     of the Russian Academy of Sciences (ISPRAS)
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

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");

/* ================================================================ */
static void
kedr_test_cleanup_module(void)
{
	return;
}

static int __init
kedr_test_init_module(void)
{
	return 0; /* success */
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/* ================================================================ */
