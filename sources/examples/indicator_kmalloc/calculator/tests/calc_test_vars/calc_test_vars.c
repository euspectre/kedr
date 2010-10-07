#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */

#include <calculator.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

char* expr = NULL;
module_param(expr, charp, S_IRUGO);
char* x_str = NULL;
module_param_named(x, x_str, charp, S_IRUGO);
char* y_str = NULL;
module_param_named(y, y_str, charp, S_IRUGO);
char str_result[10] = "";
module_param_string(result, str_result, sizeof(str_result), S_IRUGO);
// Examples of defining constants for kedr_calc
// simple
struct kedr_calc_const bool_constants[] = {
    {"TRUE", 1},
    {"FALSE", 0}
};
// usage of KEDR_C_CONSTANT
#define big_number 10000
struct kedr_calc_const my_constant = KEDR_C_CONSTANT(big_number);
// Array of all constants may reuse constants groups
struct kedr_calc_const_vec all_constants[] = {
    {sizeof(bool_constants)/sizeof(bool_constants[0]) , bool_constants},
    {1, &my_constant}
};
const int all_constants_size = sizeof(all_constants) / sizeof(all_constants[0]);
// Auxiliary function for evaluate value of expression, which not used variables.
// Return 0 on success.
static int evaluate_str(const char* str, kedr_calc_int_t *value)
{
    kedr_calc_t* calc = kedr_calc_parse(str, all_constants_size, all_constants, 0, NULL);
    if(calc == NULL)
    {
        printk(KERN_ERR "Cannot evaluate expression '%s'", str);
        return 1;
    }
    *value = kedr_calc_evaluate(calc, NULL);
    kedr_calc_delete(calc);
    return 0;
}

static int __init
calc_simple_test_init(void)
{
    int result;
    //variable names and placeholer for their values
    const char* vars[] = {"x", "y"};
    kedr_calc_int_t values[2];
    //
    kedr_calc_t* calc = NULL;
    if(expr == NULL || x_str == NULL || y_str == NULL)
    {
        pr_err("Parameters 'expr', 'x' and 'y' should be passed to the module.\n");
        return -1;
    }
    
    pr_debug("Expression is '%s'.", expr);
    
    calc = kedr_calc_parse(expr, all_constants_size, all_constants, 2, vars);

    if(calc == NULL)
    {
        printk(KERN_ERR "Cannot parse expression for some reason.\n");
        return -1;
    }
    if(evaluate_str(x_str, &values[0]))
    {
        printk(KERN_ERR "Cannot evaluate 'x'");
        kedr_calc_delete(calc);
        return -1;
    }
    if(evaluate_str(y_str, &values[1]))
    {
        pr_err("Cannot evaluate 'y'");
        kedr_calc_delete(calc);
        return -1;
    }
    result = kedr_calc_evaluate(calc, values);
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