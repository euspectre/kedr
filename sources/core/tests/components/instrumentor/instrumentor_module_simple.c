/*
 * Simple module envelope around kedr_instrumentor,
 * which export 'instrument' function and test function, which it replace
 * with another one.
 */

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

#include "kedr_instrumentor_internal.h"
#include "instrumentor_module.h"

#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");


static int test_value = 0;
module_param(test_value, int, S_IRUGO);

void test_function(int value)
{
    /*do nothing*/
}
EXPORT_SYMBOL(test_function);


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

int instrument_module(struct module* m)
{
    return kedr_instrumentor_replace_functions(m, replace_pairs);
}
EXPORT_SYMBOL(instrument_module);

void instrument_module_clean(struct module* m)
{
    kedr_instrumentor_replace_clean(m);
}
EXPORT_SYMBOL(instrument_module_clean);


static int __init
kedr_module_init(void)
{
    int result;
    
    result = kedr_instrumentor_init();
    if(result) return result;

    return 0;
}

static void __exit
kedr_module_exit(void)
{
    kedr_instrumentor_destroy();
}

module_init(kedr_module_init);
module_exit(kedr_module_exit);