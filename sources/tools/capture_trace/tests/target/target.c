/*********************************************************************
 * Module: kedr_target
 *********************************************************************/

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
 
#include <linux/module.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>	    /* kmalloc(), kfree() */

/* ================================================================ */
static void
kedr_target_cleanup_module(void)
{
    void* p = kmalloc(100, GFP_KERNEL);
    kfree(p);
	return;
}

static int __init
kedr_target_init_module(void)
{
    void* p = kmalloc(100, GFP_KERNEL);
    kfree(p);
    	
	return 0; /* success */

}

module_init(kedr_target_init_module);
module_exit(kedr_target_cleanup_module);
