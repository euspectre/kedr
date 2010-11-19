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

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <kedr/fault_simulation/calculator.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

char* expr = NULL;
module_param(expr, charp, S_IRUGO);
char str_result[10] = "";
module_param_string(result, str_result, sizeof(str_result), S_IRUGO);

static int __init
calc_simple_test_init(void)
{
    int result;
    kedr_calc_t* calc = NULL;
    if(expr == NULL)
    {
        printk(KERN_ERR "'expr' parameter should be passed to the module.\n");
        return -1;
    }
    
    printk(KERN_INFO "Expression is '%s'.", expr);
    calc = kedr_calc_parse(expr, 0, NULL, 0, NULL, 0, NULL);
    if(calc == NULL)
    {
        printk(KERN_ERR "Cannot parse expression for some reason.\n");
        return -1;
    }
    
    result = kedr_calc_evaluate(calc, NULL);
    kedr_calc_delete(calc);
    
    snprintf(str_result, sizeof(str_result), "%d", result);
    return 0;
    
}

static void
calc_simple_test_exit(void)
{
	return;
}

module_init(calc_simple_test_init);
module_exit(calc_simple_test_exit);