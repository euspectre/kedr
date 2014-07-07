/* This module uses kmalloc() and alloc_pages() to allocate several memory
 * blocks of different size, and the corresponding functions to free them. 
 * The size may be a compile-time constant or may be determined in runtime 
 * only.
 * Using LeakCheck for this module should allow to detect at least some of 
 * the changes in the implementation of kmalloc, etc. The developers seem
 * to rename the functions kmalloc expands to from time to time, change 
 * their parameters a bit and so on. Although this way to detect such 
 * changes is not 100% reliable, it may help. */
	
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

#include <linux/kernel.h> 
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
/* ====================================================================== */

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");
/* ====================================================================== */
	
/* This parameter can be used in the tests as a volatile value to prevent 
 * the compiler from over-optimizing the code (perhaps, a volatile variable
 * would also do?). The module must not change the value of this parameter.
 */
unsigned int value_one = 1;
module_param(value_one, uint, S_IRUGO);
/* ====================================================================== */

/* The maximum order of the number of pages we should try to allocate in 
 * this test. If one tries to allocate more than 2^(MAX_ORDER - 1) pages,
 * the kernel may complain. See, for example, the implementation of 
 * __alloc_pages_slowpath(). */
#define TEST_MAX_ORDER (MAX_ORDER - 1)

#define TEST_KMALLOC_SIZE(size) 	\
do { 					\
	void *buf = kmalloc((size), GFP_KERNEL); \
	kfree(buf);			\
} while (0)

/* Request up to 128 Kb, the maximum allowed size should not be less. */
#define TEST_KMALLOC_CONST_SIZE	\
do { 				\
	TEST_KMALLOC_SIZE(1);	\
	TEST_KMALLOC_SIZE(2);	\
	TEST_KMALLOC_SIZE(4);	\
	TEST_KMALLOC_SIZE(8);	\
	TEST_KMALLOC_SIZE(16);	\
	TEST_KMALLOC_SIZE(32);	\
	TEST_KMALLOC_SIZE(64);	\
	TEST_KMALLOC_SIZE(128);	\
	TEST_KMALLOC_SIZE(256);	\
	TEST_KMALLOC_SIZE(512);	\
	TEST_KMALLOC_SIZE(1024); \
	TEST_KMALLOC_SIZE(2048); \
	TEST_KMALLOC_SIZE(4096); \
	TEST_KMALLOC_SIZE(8192); \
	TEST_KMALLOC_SIZE(16384); \
	TEST_KMALLOC_SIZE(32768); \
	TEST_KMALLOC_SIZE(65536);  \
	TEST_KMALLOC_SIZE(131072); \
} while (0)

static void
do_test(void)
{
	unsigned int order = 0;
	unsigned int nr_bytes;
	struct page *page = NULL;
	
	/* Just in case */
	BUILD_BUG_ON(MAX_ORDER < 1);
	
	/* Allocating 1 and 2 pages (constant order). */
	page = alloc_pages(GFP_KERNEL, 0);
	if (page)
		__free_pages(page, 0); 
	
	page = alloc_pages(GFP_KERNEL, 1);
	if (page)
		__free_pages(page, 1); 
	
	/* kmalloc with constant block sizes */
	TEST_KMALLOC_CONST_SIZE;
	
	/* Allocating the blocks up to a page in size. */
	for (nr_bytes = 1; nr_bytes <= PAGE_SIZE; nr_bytes *= 2)
		TEST_KMALLOC_SIZE(nr_bytes * value_one);
	
	/* Allocating the blocks of one page or more in size. */
	for (order = 0; order <= TEST_MAX_ORDER; order += value_one) {
		nr_bytes = (1 << order) * PAGE_SIZE;
		TEST_KMALLOC_SIZE(nr_bytes * value_one);
		
		page = alloc_pages(GFP_KERNEL, order * value_one);
		if (page)
			__free_pages(page, order * value_one); 
	}
}

static void
kedr_test_cleanup_module(void)
{
	do_test();
}

static int __init
kedr_test_init_module(void)
{
	return 0;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/* ====================================================================== */
