/*********************************************************************
 * This module sets callbacks to be notified when the target module has 
 * just loaded (if 'set_load_fn' parameter is non-zero) or is about to 
 * unload (if 'set_unload_fn' parameter is non-zero). For the test script 
 * to be able to check if the correct 'struct module' instance has been 
 * passed to these callbacks, they get the name of the module from it and 
 * output it via parameters 'target_loaded_name' and 'target_unloaded_name'.
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
 
#include <linux/string.h>   /* kstrdup() */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/slab.h>     /* __kmalloc() */

#include <kedr/base/common.h>

/*********************************************************************/
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/*********************************************************************/

/* [in] 0 - do not set callback for "target load" event, != 0 - set it. */
static unsigned int set_load_fn = 0;
module_param(set_load_fn, uint, S_IRUGO);

/* [in] 0 - do not set callback for "target unload" event, != 0 - set it. */
static unsigned int set_unload_fn = 0;
module_param(set_unload_fn, uint, S_IRUGO);

/* I do not like assignments of constant char pointers to 'char *' even for 
 * the parameters of the modules, hence this trick with an array below. */

static char no_target[] = "<none>";

/* [out] Name of the target that has just loaded. */
static char *target_load_name = &no_target[0];
module_param(target_load_name, charp, S_IRUGO);

/* [out] Name of the target that is about to unload. */
static char *target_unload_name = &no_target[0];
module_param(target_unload_name, charp, S_IRUGO);

/*********************************************************************
 * Replacement functions
 *********************************************************************/
static void*
repl___kmalloc(size_t size, gfp_t flags)
{
	return __kmalloc(size, flags);
}

/*********************************************************************
 * Callbacks for target load / unload notifications
 *********************************************************************/
static void
target_load_callback(struct module *target_module)
{
    char *name = NULL;
    
    BUG_ON(target_module == NULL);
    name = module_name(target_module);
    
    kfree(target_load_name);
    target_load_name = kstrdup(name, GFP_KERNEL); /* If NULL - also OK */
    return;
}

static void
target_unload_callback(struct module *target_module)
{
    char *name = NULL;
    
    BUG_ON(target_module == NULL);
    name = module_name(target_module);
    
    kfree(target_unload_name);
    target_unload_name = kstrdup(name, GFP_KERNEL); /* If NULL - also OK */
    return;
}
/*********************************************************************/

/* Names and addresses of the functions of interest */
static void* orig_addrs[] = {
	(void*)&__kmalloc
};

/* Addresses of the replacement functions */
static void* repl_addrs[] = {
	(void*)&repl___kmalloc
};

static struct kedr_payload payload = {
	.mod                    = THIS_MODULE,
	.repl_table.orig_addrs  = &orig_addrs[0],
	.repl_table.repl_addrs  = &repl_addrs[0],
	.repl_table.num_addrs   = ARRAY_SIZE(orig_addrs),
    .target_load_callback   = NULL,
    .target_unload_callback = NULL
};
/*********************************************************************/

static void
kedr_test_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
    
    kfree(target_load_name);
    kfree(target_unload_name);
	return;
}

static int __init
kedr_test_init_module(void)
{
    int result = 0;
    
	BUG_ON(	ARRAY_SIZE(orig_addrs) != 
		ARRAY_SIZE(repl_addrs));
    
    target_load_name = kstrdup(&no_target[0], GFP_KERNEL);
    target_unload_name = kstrdup(&no_target[0], GFP_KERNEL);
    if (target_load_name == NULL || target_unload_name == NULL)
    {
        result = -ENOMEM;
        goto err;
    }
    
    if (set_load_fn != 0)
        payload.target_load_callback = target_load_callback;
    
    if (set_unload_fn != 0)
        payload.target_unload_callback = target_unload_callback;
    
    result = kedr_payload_register(&payload);
    if (result != 0)
        goto err;
    
	return 0;

err:
    kfree(target_load_name);
    kfree(target_unload_name);
    return result;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
