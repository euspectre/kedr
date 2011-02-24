/*********************************************************************
 * This module creates several memory leaks to be detected by LeakCheck.
 * The addresses of the leaked memory blocks are made available via
 * the parameters of the module (as unsigned long values): leak_kmalloc,
 * leak_gfp, ...
 * Some other module ("cleaner") may use these parameters to actually
 * free that blocks eventually.
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include <kedr_test_lc_common.h>
/*********************************************************************/

#define KEDR_TEST_NUM_ITEMS 12

/*********************************************************************/
MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");
/*********************************************************************/
/*
 * These parameters will be used to report the addresses of the leaked
 * memory blocks to the user space (the addresses are cast to unsigned 
 * long): 
 * - leak_kmalloc
 * - leak_gfp (for __get_free_pages)
 * - leak_vmalloc
 * - leak_kmemdup
 * 
 * The values of these parameters initially set by the user do not matter,
 * they are set to 0 in init().
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

static char some_string[] = "Pack my box with five dozen liquor jugs";
static int low_memory = 0;
/*********************************************************************/

static void
do_leak_kmalloc(void)
{
    void *item[KEDR_TEST_NUM_ITEMS + 1];
    int i;
    
    for (i = 0; i <= KEDR_TEST_NUM_ITEMS; ++i) {
        item[i] = kmalloc(ARRAY_SIZE(some_string), GFP_KERNEL);
        
/* If the allocation fails, set the flag and continue, kfree(NULL) is OK */
        if (item[i] == NULL)
            low_memory = 1; 
    }
    
    /* Leak the last allocated block */
    for (i = 0; i < KEDR_TEST_NUM_ITEMS; ++i)
        kfree(item[i]);
    
    leak_kmalloc = (unsigned long)item[KEDR_TEST_NUM_ITEMS];
}

static void
do_leak_gfp(void)
{
    unsigned long p = __get_free_pages(GFP_KERNEL, KEDR_GFP_ORDER);
    if (p == 0)
        low_memory = 1;
    
    leak_gfp = p;
}

static void
do_leak_vmalloc(void)
{
    void *p = vmalloc(ARRAY_SIZE(some_string));
    if (p == NULL)
        low_memory = 1;
    
    leak_vmalloc = (unsigned long)p;
}

static void
do_leak_kmemdup(void)
{
    void *p = kmemdup(&some_string[0], sizeof(some_string), GFP_KERNEL);
    if (p == NULL)
        low_memory = 1;
    
    leak_kmemdup = (unsigned long)p;
}
/*********************************************************************/
static void
kedr_test_cleanup_module(void)
{
}

static int __init
kedr_test_init_module(void)
{
    leak_kmalloc = 0;
    leak_gfp = 0;
    leak_vmalloc = 0;
    leak_kmemdup = 0;
    
    do_leak_gfp();
    do_leak_vmalloc();
    do_leak_kmemdup();
    do_leak_kmalloc();
	
	if (low_memory) 
        goto fail;
	
	return 0;

fail:
    if (leak_kmalloc)
        kfree((void *)leak_kmalloc);
    
    if (leak_gfp)
        free_pages(leak_gfp, KEDR_GFP_ORDER);
    
    if (leak_vmalloc)
        vfree((void *)leak_vmalloc);
    
    if (leak_kmemdup)
        kfree((void *)leak_kmemdup);
        
    return -ENOMEM;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
