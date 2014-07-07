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
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");

/* parameters */
int in_init = 0;
int in_atomic = 0;

module_param(in_init, int, S_IRUGO);
module_param(in_atomic, int, S_IRUGO);

/* A spinlock to simulate an atomic context */
DEFINE_SPINLOCK(test_lock);

/* ================================================================ */
static void
do_test_alloc(void)
{
    void *ptr = NULL;
    ptr = __kmalloc(sizeof(int), GFP_ATOMIC);
    if (ptr == NULL)
    {
        printk(KERN_ALERT "[test_in_init_target] "
            "Failed to allocate memory\n"
        );
        return;
    }
    
    kfree(ptr);
    return;
}

static void
test_alloc(void)
{
    if (in_atomic)
    {
        unsigned long flags;
        spin_lock_irqsave(&test_lock, flags);
        do_test_alloc();
        spin_unlock_irqrestore(&test_lock, flags);
    }
    else
    {
        do_test_alloc();
    }
    return;
}

static void
kedr_test_cleanup_module(void)
{
    if (!in_init)
    {
        test_alloc();
    }
	return;
}

static int __init
kedr_test_init_module(void)
{
    if (in_init)
    {
        test_alloc();
    }
	return 0; /* success */
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/* ================================================================ */
