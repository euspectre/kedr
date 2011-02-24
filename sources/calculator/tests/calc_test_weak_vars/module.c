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

#include <linux/kernel.h>	/* printk() */

#include <kedr/calculator/calculator.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

char* expr = NULL;
module_param(expr, charp, S_IRUGO);
//some 'complex' expression, which evaluation may be performed, if needed
char* sub_expr = NULL;
module_param(sub_expr, charp, S_IRUGO);

char str_result[10] = "";
module_param_string(result, str_result, sizeof(str_result), S_IRUGO);


//Whether error occurs while parse 'complex' expression
int sub_expr_error = 0;
//
static kedr_calc_int_t sub_expr_compute(void)
{
    int result;
    kedr_calc_t* calc = NULL;
    if(sub_expr == NULL)
    {
        pr_err("Parameter 'sub_expr' should be passed to the module.\n");
        sub_expr_error = 1;
        return 0;
    }
    calc = kedr_calc_parse(sub_expr, 0, NULL, 0, NULL, 0, NULL);
    if(calc == NULL)
    {
        pr_err("Cannot parse subexpression for some reason.");
        sub_expr_error = 1;
        return 0;
    }
    result = kedr_calc_evaluate(calc, NULL);
    kedr_calc_delete(calc);

    return result;
}

struct kedr_calc_weak_var sub_expr_weak_var =
{
    .name = "sub_expr",
    .compute = sub_expr_compute
};

static int __init
this_module_init(void)
{
    int result;
    //
    kedr_calc_t* calc = NULL;
    if(expr == NULL)
    {
        pr_err("Parameter 'expr' should be passed to the module.");
        return -1;
    }
    
    pr_debug("Expression is '%s'.", expr);
    
    calc = kedr_calc_parse(expr, 0, NULL, 0, NULL, 1, &sub_expr_weak_var);

    if(calc == NULL)
    {
        pr_err("Cannot parse expression for some reason.");
        return -1;
    }
    result = kedr_calc_evaluate(calc, NULL);
    kedr_calc_delete(calc);
    
    if(sub_expr_error)
    {
        pr_err("Error occurs while computing subexpression.");
        return -1;
    }
    
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