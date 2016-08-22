/* ========================================================================
 * Copyright (C) 2016, Evgenii Shatokhin <eugene.shatokhin@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/kedr.h>

#ifndef TESTME
# error TESTME is not defined
#endif

#ifndef CONFIG_KEDR_LEAK_CHECKER
# error CONFIG_KEDR_LEAK_CHECKER is not defined
#endif

MODULE_AUTHOR("Evgenii Shatokhin");
MODULE_LICENSE("GPL");
/* ================================================================ */

static int __init
kedr_init(void)
{
	pr_info("[DBG] TESTME is %u.\n", TESTME);
	return 0;
}

static void __exit
kedr_exit(void)
{
	return;
}

module_init(kedr_init);
module_exit(kedr_exit);
/* ================================================================ */
