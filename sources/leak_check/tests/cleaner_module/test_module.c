/*********************************************************************
 * This module cleans up memory leaks (frees the memory blocks) passed
 * to it as parameters. The leaks are made by another module ("leaker").
 *
 * In addition, this allows to verify that LeakCheck detects "unallocated 
 * frees" too.
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>

#include <kedr_test_lc_common.h>

/*********************************************************************/
MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");
/*********************************************************************/
/*
 * These parameters will be used to get the addresses of the leaked
 * memory blocks that should be freed (the addresses are cast to unsigned 
 * long): 
 * - leak_kmalloc
 * - leak_gfp (for __get_free_pages)
 * - leak_vmalloc
 * - leak_kmemdup
 */
unsigned long leak_kmalloc = 0;
module_param(leak_kmalloc, ulong, S_IRUGO);

unsigned long leak_gfp = 0;
module_param(leak_gfp, ulong, S_IRUGO);

unsigned long leak_vmalloc = 0;
module_param(leak_vmalloc, ulong, S_IRUGO);

unsigned long leak_kmemdup = 0;
module_param(leak_kmemdup, ulong, S_IRUGO);
/*********************************************************************/

static void
kedr_test_cleanup_module(void)
{
}

static int __init
kedr_test_init_module(void)
{
    if (leak_kmalloc)
        kfree((void *)leak_kmalloc);
    
    if (leak_gfp)
        free_pages(leak_gfp, KEDR_GFP_ORDER);
    
    if (leak_vmalloc)
        vfree((void *)leak_vmalloc);
    
    if (leak_kmemdup)
        kfree((void *)leak_kmemdup);
	
	return 0;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
