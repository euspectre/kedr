/*********************************************************************
 * This module uses kfree_rcu to perform a (probably deferred) deletion
 * of an RCU-protected structure.
 *********************************************************************/
/* ========================================================================
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
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
/*********************************************************************/

MODULE_AUTHOR("Eugene");
MODULE_LICENSE("GPL");
/*********************************************************************/

struct kedr_foo
{
	void *baz_not_used;
	struct rcu_head rcu_head;
};

struct kedr_foo *foo = NULL;
struct kedr_foo *bar = NULL;
/*********************************************************************/

static void 
kedr_test_rcu_callback(struct rcu_head *rcu_head)
{
	struct kedr_foo *kedr_foo; 
	
	BUG_ON(rcu_head == NULL);
	
	kedr_foo = container_of(rcu_head, struct kedr_foo, rcu_head);
	kfree(kedr_foo);
} 

static void
kedr_test_cleanup_module(void)
{
	/* This is to make sure the RCU callbacks set by kfree_rcu() and
	 * call_rcu() have completed before the module is unloaded. */
	rcu_barrier();
}

static int __init
kedr_test_init_module(void)
{
	foo = kzalloc(sizeof(struct kedr_foo), GFP_KERNEL);
	if (foo == NULL)
		return -ENOMEM;
	
	bar = kzalloc(sizeof(struct kedr_foo), GFP_KERNEL);
	if (bar == NULL) {
		kfree(foo);
		return -ENOMEM;
	}
	
	call_rcu(&bar->rcu_head, kedr_test_rcu_callback);
	kfree_rcu(foo, rcu_head);
	return 0;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
