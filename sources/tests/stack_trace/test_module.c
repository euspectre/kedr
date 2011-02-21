/*********************************************************************
 * The module finds the number of reliable stack frames that can be 
 * obtained using kedr_save_stack_trace.
 * The result is made available to user space via "stack_frames"
 * parameter.
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

#include <kedr/util/stack_trace.h>

/*********************************************************************/
MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");
/*********************************************************************/

/* 
 * "stack_frames" parameter.
 */
unsigned long stack_frames = 0;
module_param(stack_frames, ulong, S_IRUGO);

/*
 * addr_foo - address of kedr_test_foo() function cast to (unsigned long).
 * This is just to use these addresses somehow which (in addition to export)
 * may help prevent inlining.
 */
unsigned long addr_foo = 0;
module_param(addr_foo, ulong, S_IRUGO);
/*********************************************************************/

void
kedr_test_foo(void)
{
    unsigned long entries[KEDR_MAX_FRAMES];
    unsigned int max_entries = KEDR_MAX_FRAMES;
    unsigned int nr_entries = 0;
    
    addr_foo = (unsigned long)(&kedr_test_foo);

    kedr_save_stack_trace(&entries[0], max_entries, &nr_entries);
    stack_frames = (unsigned long)nr_entries;
    return;
}
EXPORT_SYMBOL(kedr_test_foo);

/*********************************************************************/
static void
kedr_test_cleanup_module(void)
{
}

static int __init
kedr_test_init_module(void)
{
    addr_foo = (unsigned long)(&kedr_test_foo);
    kedr_test_foo();
	return 0;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
