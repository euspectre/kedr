/*
 * Calculator for integer operations.
 *
 * Perform evaluation of string expression, which may contains
 * numbers, operations, named constants and named variables.
 *
 * Evaluation is performed in two steps:
 * 1) parse expression, substituting values of constants, and create internal representation of the expression.
 * 2) calculate value of the expression, substituting values of variables.
 *
 * Key factor of this division in that the second step is fast and may be used in atomic context.
 *
 * This calculator may be used, e.g., in indicator functions.
 */

/*
 * Supported operations and their priorities:
 *
 * "!a", "~a"                   (1)
 * "+a", "-a"                   (2)
 * "a*b", "a/b", "a%b"          (3)
 * "a+b", "a-b"                 (4)
 * "a>>b", "a<<b"               (5)
 * "a<b", "a>b", "a<=b", "a>=b" (6)
 * "a=b", "a!=b"                (7) // '=' in the sence of '=='
 * "a&b"                        (8)
 * "a^b"                        (9)
 * "a|b"                        (10)
 * "a&&b"                       (11)
 * "a||b"                       (12)
 * "a?b:c"                      (13)
 *
 * All operations except ?: has left-to-right grouping, ?: has right-to-left-grouping.
 */


 
#ifndef KEDR_CALCULATOR_H
#define KEDR_CALCULATOR_H

/*
 * Integer type, which used in the expression.
 *
 * Should support negative values and be convertable to int.
 */

typedef long kedr_calc_int_t;

// Internal representation of expression, used at the second step of its evaluation.
typedef struct kedr_calc kedr_calc_t;

// Name-value pair, describing one constant.
struct kedr_calc_const
{
    const char* name;
    kedr_calc_int_t value;
};
// Macro for define struct kedr_calc_const for c-constant.
#define KEDR_C_CONSTANT(name) {#name, name}

/*
 * Vector, containg one or more constants definitions.
 *
 * May be used as grouping element of the constants in the same area.
 */
 
struct kedr_calc_const_vec
{
    int n_elems;
    const struct kedr_calc_const* elems;
};

/*
 * In the middle of constants and variables.
 *
 * Like constant, this parameter should be pointed only on parse() stage
 * (not at evaluate stage), but, like variable, this parameter will be computed
 * only at evaluate stage.
 *
 * Key factor - computation of such parameter is delayed until evaluate stage and
 * computation only when it really used in expression.
 *
 * Useful for gathering some runtime information about process, or for variable,
 * which take a long time to compute.
 */

struct kedr_calc_weak_var
{
    const char* name;
    kedr_calc_int_t (*compute)(void);
};


/*
 * Parse given expression, which may contain given constans and variables,
 * and create its internal representation.
 *
 * Return internal representation of the expression,
 * or NULL, if expression is incorrect or other error occurs.
 *
 * 'const_vec' - array of constant definitions,
 * 'const_vec_n' - number of elements in it.
 *
 * 'var_names' - ordered array of strings(const char*), representing names of variables,
 * 'var_n' - number of elements in it.
 */

kedr_calc_t* 
kedr_calc_parse(const char* expr,
    int const_vec_n, const struct kedr_calc_const_vec* const_vec,
    int var_n, const char* const* var_names,
    int weak_vars_n, const struct kedr_calc_weak_var* weak_vars);

/*
 * Evaluate internal representation of the expression with given values of variables.
 *
 * 'var_values' is array of variables values in the same order,
 * as 'var_names' array, passed into kedr_calc_parse() function.
 */

kedr_calc_int_t kedr_calc_evaluate(const kedr_calc_t* calc, const kedr_calc_int_t* var_values);

/*
 * Remove internal represenation of expression, and free all resources, used by it.
 */

void kedr_calc_delete(kedr_calc_t* calc);

#endif /*KEDR_CALCULATOR_H*/
