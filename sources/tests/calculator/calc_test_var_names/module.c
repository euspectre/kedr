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

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */

#include <kedr/calculator/calculator.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

char* expr = NULL;
module_param(expr, charp, S_IRUGO);
//names of two variables, which may be used in expression.
// Value of the first one is 1, second - 2
char* var_names[2];
module_param_named(var_name1, var_names[0], charp, S_IRUGO);
module_param_named(var_name2, var_names[1], charp, S_IRUGO);

char str_result[10] = "";
module_param_string(result, str_result, sizeof(str_result), S_IRUGO);


static int __init
this_module_init(void)
{
    int result;
    kedr_calc_int_t vars[2] = {1,2};

    kedr_calc_t* calc = NULL;
    if((expr == NULL) || (var_names[0] == NULL) || (var_names[1] == NULL))
    {
        pr_err("Parameter 'expr' should be passed to the module, as the names of two variables.\n");
        return -1;
    }
    
    pr_debug("Expression is '%s'.", expr);
    
    calc = kedr_calc_parse(expr, 0, NULL, 2, (const char**)var_names, 0, NULL);

    if(calc == NULL)
    {
        pr_err("Cannot parse expression for some reason.\n");
        return -1;
    }
    result = kedr_calc_evaluate(calc, vars);
    kedr_calc_delete(calc);
    
    snprintf(str_result, sizeof(str_result), "%d", result);
    return 0;
}

static void
this_module_exit(void)
{
	return;
}

module_init(this_module_init);
module_exit(this_module_exit);