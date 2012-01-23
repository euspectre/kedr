/*
 * Simple module envelope around kedr_instrumentor,
 * which detect loading of target and instrument it, replacing test function
 * with another one.
 */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
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

#include "kedr_instrumentor_internal.h"
#include "instrumentor_module.h"

#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

/* name of the module to instrument when it will be loaded. */
char* target_name = NULL;
module_param(target_name, charp, S_IRUGO);

/* stored pointer to the instrumented module*/
struct module* target_module = NULL;

/* value which is set by replacement function*/
static int test_value = 0;
module_param(test_value, int, S_IRUGO);


/* Function which is called by the instrumented module...*/
void test_function(int value)
{
    /*do nothing*/
}
EXPORT_SYMBOL(test_function);


/* ... and one which should be called really. */
static void test_function_repl(int value)
{
    test_value = value;
}

static struct kedr_instrumentor_replace_pair replace_pairs[] =
{
    {
        .orig = test_function,
        .repl = test_function_repl,
    },
    {
        .orig = NULL
    }
};


/* ================================================================== */
/* A callback function to catch loading and unloading of module. 
 * When detect module interested us, instrument it. */
static int 
detector_notifier_call(struct notifier_block *nb,
    unsigned long mod_state, void *vmod)
{
    struct module* mod = (struct module *)vmod;
    BUG_ON(mod == NULL);
    
    /* handle module state change */
    switch(mod_state)
    {
    case MODULE_STATE_COMING: /* the module has just loaded */
        if(strcmp(target_name, module_name(mod)) == 0)
        {
            BUG_ON(target_module != NULL);
            if(!kedr_instrumentor_replace_functions(mod, replace_pairs))
            {
                target_module = mod;
            }
            else
            {
                pr_err("Fail to instrument module.");
            }
        }
    break;
    
    case MODULE_STATE_GOING: /* the module is going to unload */
        if(mod == target_module)
        {
            kedr_instrumentor_replace_clean(mod);
            target_module = NULL;
        }
    break;
    }

    return 0;
}

/* ================================================================ */
/* A struct for watching for loading/unloading of modules.*/
static struct notifier_block detector_nb = {
    .notifier_call = detector_notifier_call,
    .next = NULL,
    .priority = 3, /*Some number*/
};


static int __init
kedr_module_init(void)
{
    int result;
    
    result = kedr_instrumentor_init();
    if(result) return result;
    
    result = register_module_notifier(&detector_nb);
    if(result)
    {
        kedr_instrumentor_destroy();
        return result;
    }

    return 0;
}

static void __exit
kedr_module_exit(void)
{
    unregister_module_notifier(&detector_nb);
    kedr_instrumentor_destroy();
}

module_init(kedr_module_init);
module_exit(kedr_module_exit);