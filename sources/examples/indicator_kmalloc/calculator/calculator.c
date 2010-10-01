// Implementation of the expression evaluator.

#include "calculator.h"

#include <linux/slab.h> /* kmalloc & kfree */
#include <linux/ctype.h> /* character classes*/

// Macros for unify output information to the kernel log file
#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)


/* 
 * Priorities as ordered numbers.
 *
 * This numbers also resolve grouping(left-to-right or right-to-left)
 * of operations with same priority(e.g, same operation).
 *
 * This resolution is done in means of difference of priority of left operand and right operands
 * of operation.
 *
 * E.g., assume that for operation '#' priority_left < priority_right.
 * Then a#b#c will be treated as (a#b)#c (left-to-right grouping),
 * because operand 'b' 'attracts greater' to the left '#' (as its right operand),
 * than to the right '#'(as its left operand).
 *
 * Revers order of operands' priorities means reverse grouping.
 *
 * Some operations(e.g unary '-') has only right operand, so them have only one priority number assigned.
 *
 * '(' may be considered as unary operation, which right operand has minimum priority
 * (so, this operand will 'attracts' to any right operation, except delimiters), but require ')' delimiter
 * at the end of expression which it affect.
 *
 * '?' operation have left operand (everything before '?'), medium, which has also minimum priority,
 * but require ':' delimiter at the end, and right operand, which starts after ':'.
 */

enum priorities
{
    priority_min = 0, //used for expressions between delimiters(between '('and ')', or between '?' and ':')

    priority_cond_right,
    priority_cond_left, // left>right, because '?' operator has right-to-left grouping

    priority_logical_or_left,
    priority_logical_or_right,

    priority_logical_and_left,
    priority_logical_and_right,

    priority_binary_or_left,
    priority_binary_or_right,

    priority_binary_xor_left,
    priority_binary_xor_right,

    priority_binary_and_left,
    priority_binary_and_right,

    priority_equal_left,
    priority_inequal_left = priority_equal_left,
    priority_equal_right,
    priority_inequal_right = priority_equal_right,

    priority_less_left,
    priority_greater_left = priority_less_left,
    priority_less_equal_left = priority_less_left,
    priority_greater_equal_left = priority_less_left,
    priority_less_right,
    priority_greater_right = priority_less_right,
    priority_less_equal_right = priority_less_right,
    priority_greater_equal_right = priority_less_right,

    priority_left_shift_left,
    priority_right_shift_left = priority_left_shift_left,
    priority_left_shift_right,
    priority_right_shift_right = priority_left_shift_right,

    priority_plus_left, //priority of the left operand of binary '+' operation
    priority_minus_left = priority_plus_left,
    priority_plus_right,
    priority_minus_right = priority_plus_right,

    priority_multiply_left,
    priority_divide_left = priority_multiply_left,
    priority_rest_left = priority_multiply_left,
    priority_multiply_right,
    priority_divide_right = priority_multiply_right,
    priority_rest_right = priority_multiply_right,

    priority_unary_plus, //only left priority
    priority_unary_minus = priority_unary_plus,

    priority_logical_not,
    priority_binary_not = priority_logical_not
};

//Essences, used in the internal representation of the expression.
enum calc_essence_type
{
    calc_essence_type_value,
    calc_essence_type_variable,

    calc_essence_type_binary_not,
    calc_essence_type_logical_not,

    calc_essence_type_unary_plus,
    calc_essence_type_unary_minus,

    calc_essence_type_multiply,
    calc_essence_type_divide,
    calc_essence_type_rest,

    calc_essence_type_plus,
    calc_essence_type_minus,
    
    calc_essence_type_left_shift,
    calc_essence_type_right_shift,
    
    calc_essence_type_less,
    calc_essence_type_greater,
    calc_essence_type_less_equal,
    calc_essence_type_greater_equal,
    
    calc_essence_type_equal,
    calc_essence_type_inequal,
    
    calc_essence_type_binary_and,
    calc_essence_type_binary_xor,
    calc_essence_type_binary_or,
    
    calc_essence_type_logical_and,
    calc_essence_type_logical_or,
        
    calc_essence_type_cond
    //and so on..
};
//'virtual base class' of essences
struct calc_essence
{
    enum calc_essence_type type;
};

//Real essences
//Essence which evaluated as constant value
struct calc_essence_val
{
    struct calc_essence base;
    kedr_calc_int_t value;
};
//Essence which evaluated as variable with given index
struct calc_essence_var
{
    struct calc_essence base;
    unsigned int index;
};
//Essence which evaluated as some unary operation of its operand
struct calc_essence_1op
{
    struct calc_essence base;
    struct calc_essence* op;
};
//Essence which evaluated as some binary operation of its operands
struct calc_essence_2op
{
    struct calc_essence base;
    struct calc_essence* op1;
    struct calc_essence* op2;
};
//Essence which evaluated as ternary operation of its operand
struct calc_essence_3op
{
    struct calc_essence base;
    struct calc_essence* op1;
    struct calc_essence* op2;
    struct calc_essence* op3;
};
//Create filled essence. If fail, trace error and return NULL.

//Common part of allocating algorithm - need for trace common errors in allocation process
static void*
calc_essence_alloc(size_t size);

//Create filled instance of calc_essence_val
static struct calc_essence*
calc_essence_val_create(kedr_calc_int_t value);
//Create filled instance of calc_essence_var
static struct calc_essence*
calc_essence_var_create(unsigned int index);
//Create filled instance of calc_essence_1op
static struct calc_essence*
calc_essence_1op_create(enum calc_essence_type type, struct calc_essence* op);
//Create filled instance of calc_essence_2op
static struct calc_essence*
calc_essence_2op_create(enum calc_essence_type type, struct calc_essence* op1, struct calc_essence* op2);
//Create filled instance of calc_essence_3op
static struct calc_essence*
calc_essence_3op_create(enum calc_essence_type type,
    struct calc_essence* op1, struct calc_essence* op2, struct calc_essence* op3);

// Evaluate(possibly, recursively) essence, using given values of variables
static kedr_calc_int_t calc_essence_evaluate(const struct calc_essence* essence, const kedr_calc_int_t* var_values);
// Free(possibly, recursively) all resources, used by essence.
static void calc_essence_free(struct calc_essence* essence);

//Object which used at evaluate stage.
struct kedr_calc
{
    //only reference to the top-most essence
    struct calc_essence* top_essence;
};
//Type of tokens, used in parsing process
enum token_type
{
    token_type_error = 0, // error indicator

    token_type_start, //state before first parse_data_next_token() call
    token_type_eof, // indicator end of string

    token_type_value, // immediate value
    token_type_constant, // named constant
    token_type_variable, // variable

    token_type_open_parenthesis, // '('
    token_type_close_parenthesis, // ')'

    token_type_logical_not, // '!'
    token_type_binary_not, // '~'

    //token_type_unary_plus, // '+'
    //token_type_unary_minus, // '-'

    token_type_multiply, // '*'
    token_type_divide, // '/'
    token_type_rest, // '%'

    token_type_plus, // '+'
    token_type_minus, // '-'

    token_type_left_shift, // '<<'
    token_type_right_shift, // '>>'

    token_type_less, // '<'
    token_type_greater, // '>'
    token_type_less_equal, // '<='
    token_type_greater_equal, // '>='

    token_type_equal, // '=='
    token_type_inequal, // '!='

    token_type_binary_and, // '&'
    token_type_binary_xor, // '^'
    token_type_binary_or, // '|'

    token_type_logical_and, // '&&'
    token_type_logical_or, // '||'

    token_type_cond_first, // '?'
    token_type_cond_second, // ':'

};
//Auxiliary structure, joined all data needed for parsing expression
struct parse_data
{
    const char* expr;
    int current_pos;//position of first character in the expression after recognized token (exception - null-character)
    //copy of kedr_calc_parse parameters
    int const_vec_n;
    const struct kedr_calc_const_vec* const_vec;
    
    int var_n;
    const char* const* var_names;
    // current token..
    enum token_type current_token_type;
    // ..and its additional caracteristic, if needed
    union
    {
        kedr_calc_int_t current_token_value;//for value and constant type of token
        int current_token_index;//for variable token type
    };
};
/*
 * Advance to the next token in the string and return its type.
 * Shouldn't be called in error or eof state(token_type_error or token_type_eof types of current token)
 */
static enum token_type
parse_data_next_token(struct parse_data* data);

/*
 * Parse expression in the data from the next token and return top-level essence of this expression.
 * Used recursively.
 *
 * 'priority' is affected on operations, up to which expression will be parsed.
 *
 * After function returns, current token is pointed to the first token, which is not processed.
 *
 * ...'current-token before call' ['fully processed tokens'] 'current token after return'...
 */

static struct calc_essence* 
parse_data_parse(struct parse_data* data, int priority);

/*
 * Look for constant with given name.
 *
 * On success return 0, and set 'value' to the value of the constant.
 * If constant with this name is not exist, return not 0.
 */

static int 
parse_data_search_const(const struct parse_data* data,
    const char* constant_name, int constant_name_len, kedr_calc_int_t* value);

/*
 * Look for variable with given name.
 *
 * On success return 0, and set 'index' to the index of this variable in the array.
 * If variable with this name is not exist, return not 0.
 */

static int 
parse_data_search_variable(const struct parse_data* data,
    const char* variable_name, int variable_name_len, int* index);

/* 
 * Get value of string representation of the number
 * Return 0 on success, print error message and return not 0 if number cannot be parsed.
 */

int parse_number(const char* number, int number_len, kedr_calc_int_t* value);

/////////////Implementation of exported functions//////////////////////

/*
 * Parse given expression, which may contain given constans and variables,
 * and create its internal representation.
 *
 * Return internal representation of the expression,
 * or NULL, if expression is incorrect or other error occures.
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
    int var_n, const char* const* var_names)
{
    struct kedr_calc* calc = NULL;
    struct parse_data parse_data;

    parse_data.expr = expr;
    parse_data.current_pos = 0;
    
    parse_data.const_vec_n = const_vec_n;
    parse_data.const_vec = const_vec;
    
    parse_data.var_n = var_n;
    parse_data.var_names = var_names;
    
    parse_data.current_token_type = token_type_start;
    //value and index undefined - shouln't be used with current token_type
    calc = kmalloc(sizeof(*calc), GFP_KERNEL);
    if(calc == NULL)
    {
        print_error0("Cannot allocate kedr_calc_t object.");
        return NULL;
    }
    calc->top_essence = parse_data_parse(&parse_data, priority_min);
    if(calc->top_essence == NULL)
    {
        kfree(calc);
        return NULL;//error already been traced in parse_data_parse()
    }
    if(parse_data.current_token_type != token_type_eof)
    {
        print_error("Unexpected symbol of type %d after expression.",
            (int)parse_data.current_token_type);
        calc_essence_free(calc->top_essence);
        kfree(calc);
        return NULL;
    }
    return calc;
}

/*
 * Evaluate internal representation of the expression with given values of variables.
 *
 * 'var_values' is array of variables values in the same order,
 * as 'var_names' array, passed into kedr_calc_parse() function.
 */

kedr_calc_int_t kedr_calc_evaluate(const kedr_calc_t* calc, const kedr_calc_int_t* var_values)
{
    //simply call evaluate for its 'top_essence' field
    return calc_essence_evaluate(calc->top_essence, var_values);
}

/*
 * Remove internal represenation of expression, and free all resources, used by it.
 */

void kedr_calc_delete(kedr_calc_t* calc)
{
    calc_essence_free(calc->top_essence);
    kfree(calc);
}

////////////Implementation of auxiliary functions//////////////////////////////
void*
calc_essence_alloc(size_t size)
{
    void* essence;
    if(size < sizeof(struct calc_essence*))
    {
        print_error0("Size of essence created should be greater or equal, than size of base class.");
        return NULL;
    }
    essence = kmalloc(size, GFP_KERNEL);
    if(!essence)
    {
        print_error0("Cannot allocate memory for essence.");
    }
    return essence;
}
//Create filled instance of calc_essence_val
struct calc_essence*
calc_essence_val_create(kedr_calc_int_t value)
{
    struct calc_essence_val* result = calc_essence_alloc(sizeof(*result));
    if(result == NULL) return NULL;
    result->base.type = calc_essence_type_value;
    result->value = value;
    return (struct calc_essence*)result;
}
//Create filled instance of calc_essence_var
struct calc_essence*
calc_essence_var_create(unsigned int index)
{
    struct calc_essence_var* result = calc_essence_alloc(sizeof(*result));
    if(result == NULL) return NULL;
    result->base.type = calc_essence_type_variable;
    result->index = index;
    return (struct calc_essence*)result;
}
//Create filled instance of calc_essence_1op
struct calc_essence*
calc_essence_1op_create(enum calc_essence_type type, struct calc_essence* op)
{
    struct calc_essence_1op* result;
    // Verification, whether type really represent type of unary operation
    switch(type)
    {
    case calc_essence_type_unary_plus:
    case calc_essence_type_unary_minus:
    case calc_essence_type_binary_not:
    case calc_essence_type_logical_not:
        break;
    default:
        print_error0("Unknown type of 1-operand essence");
        return NULL;
    }
    result = calc_essence_alloc(sizeof(*result));
    if(result == NULL) return NULL;
    result->base.type = type;
    result->op = op;
    return (struct calc_essence*)result;
}
//Create filled instance of calc_essence_2op
struct calc_essence*
calc_essence_2op_create(enum calc_essence_type type, struct calc_essence* op1, struct calc_essence* op2)
{
    struct calc_essence_2op* result;
    // Verification, whether type really represent type of binary operation
    switch(type)
    {
    case calc_essence_type_multiply:
    case calc_essence_type_divide:
    case calc_essence_type_rest:

    case calc_essence_type_plus:
    case calc_essence_type_minus:

    case calc_essence_type_left_shift:
    case calc_essence_type_right_shift:

    case calc_essence_type_less:
    case calc_essence_type_greater:
    case calc_essence_type_less_equal:
    case calc_essence_type_greater_equal:

    case calc_essence_type_equal:
    case calc_essence_type_inequal:

    case calc_essence_type_binary_and:
    case calc_essence_type_binary_xor:
    case calc_essence_type_binary_or:

    case calc_essence_type_logical_and:
    case calc_essence_type_logical_or:
        break;
    default:
        print_error0("Unknown type of 2-operand essence");
        return NULL;
    }
    result = calc_essence_alloc(sizeof(*result));
    if(result == NULL) return NULL;
    result->base.type = type;
    result->op1 = op1;
    result->op2 = op2;
    return (struct calc_essence*)result;
}
//Create filled instance of calc_essence_3op
struct calc_essence*
calc_essence_3op_create(enum calc_essence_type type,
    struct calc_essence* op1, struct calc_essence* op2, struct calc_essence* op3)
{
    struct calc_essence_3op* result;
    // Verification, whether type really represent type of ternary operation
    switch(type)
    {
    case calc_essence_type_cond:
        break;
    default:
        print_error0("Unknown type of 3-operand essence");
        return NULL;
    }
    result = calc_essence_alloc(sizeof(*result));
    if(result == NULL) return NULL;
    result->base.type = type;
    result->op1 = op1;
    result->op2 = op2;
    result->op3 = op3;
    return (struct calc_essence*)result;
}
//
kedr_calc_int_t 
calc_essence_evaluate(const struct calc_essence* essence, const kedr_calc_int_t* var_values)
{
    switch(essence->type)
    {
    case calc_essence_type_value:
        return ((struct calc_essence_val*)essence)->value;
    case calc_essence_type_variable:
        return var_values[((struct calc_essence_var*)essence)->index];
// Helper macro for calculate value of operand for one-operand essence
#define OP1_1 (calc_essence_evaluate(((struct calc_essence_1op*)(essence))->op, var_values))
    case calc_essence_type_unary_plus:
        return OP1_1;
    case calc_essence_type_unary_minus:
        return -OP1_1;

    case calc_essence_type_binary_not:
        return ~OP1_1;
    case calc_essence_type_logical_not:
        return !OP1_1;
#undef OP1_1
// Same for two-operand essence
#define OP2_1 (calc_essence_evaluate(((struct calc_essence_2op*)(essence))->op1, var_values))
#define OP2_2 (calc_essence_evaluate(((struct calc_essence_2op*)(essence))->op2, var_values))
    case calc_essence_type_multiply:
        return OP2_1 * OP2_2;
    case calc_essence_type_divide:
        return OP2_1 / OP2_2;
    case calc_essence_type_rest:
        return OP2_1 % OP2_2;

    case calc_essence_type_plus:
        return OP2_1 + OP2_2;
    case calc_essence_type_minus:
        return OP2_1 - OP2_2;

    case calc_essence_type_left_shift:
        return OP2_1 << OP2_2;
    case calc_essence_type_right_shift:
        return OP2_1 >> OP2_2;

    case calc_essence_type_less:
        return OP2_1 < OP2_2;
    case calc_essence_type_greater:
        return OP2_1 > OP2_2;
    case calc_essence_type_less_equal:
        return OP2_1 <= OP2_2;
    case calc_essence_type_greater_equal:
        return OP2_1 >= OP2_2;

    case calc_essence_type_equal:
        return OP2_1 == OP2_2;
    case calc_essence_type_inequal:
        return OP2_1 != OP2_2;

    case calc_essence_type_binary_and:
        return OP2_1 & OP2_2;
    case calc_essence_type_binary_xor:
        return OP2_1 ^ OP2_2;
    case calc_essence_type_binary_or:
        return OP2_1 | OP2_2;

    case calc_essence_type_logical_and:
        return OP2_1 && OP2_2;
    case calc_essence_type_logical_or:
        return OP2_1 || OP2_2;
#undef OP2_1
#undef OP2_2
// Same for three-operand essence
#define OP3_1 (calc_essence_evaluate(((struct calc_essence_3op*)(essence))->op1, var_values))
#define OP3_2 (calc_essence_evaluate(((struct calc_essence_3op*)(essence))->op2, var_values))
#define OP3_3 (calc_essence_evaluate(((struct calc_essence_3op*)(essence))->op3, var_values))
    case calc_essence_type_cond:
        return OP3_1 ? OP3_2 : OP3_3;
#undef OP3_1
#undef OP3_2
#undef OP3_3
    default:
        print_error("Unknown type of essence: %d.", essence->type);
        BUG();
        return 0;
    }
}

void
calc_essence_free(struct calc_essence* essence)
{
    WARN_ON(essence == NULL);
    if(essence == NULL) return;
    switch(essence->type)
    {
    //essences without operands
    case calc_essence_type_value:
    case calc_essence_type_variable:
        break;
    //essences with one operand
    case calc_essence_type_unary_plus:
    case calc_essence_type_unary_minus:
    case calc_essence_type_binary_not:
    case calc_essence_type_logical_not:
        calc_essence_free(((struct calc_essence_1op*)essence)->op);
        break;
    //essences with two operands
    case calc_essence_type_multiply:
    case calc_essence_type_divide:
    case calc_essence_type_rest:

    case calc_essence_type_plus:
    case calc_essence_type_minus:

    case calc_essence_type_left_shift:
    case calc_essence_type_right_shift:

    case calc_essence_type_less:
    case calc_essence_type_greater:
    case calc_essence_type_less_equal:
    case calc_essence_type_greater_equal:

    case calc_essence_type_equal:
    case calc_essence_type_inequal:

    case calc_essence_type_binary_and:
    case calc_essence_type_binary_xor:
    case calc_essence_type_binary_or:

    case calc_essence_type_logical_and:
    case calc_essence_type_logical_or:
        calc_essence_free(((struct calc_essence_2op*)essence)->op1);
        calc_essence_free(((struct calc_essence_2op*)essence)->op2);
        break;
    //essence with free operands
    case calc_essence_type_cond:
        calc_essence_free(((struct calc_essence_3op*)essence)->op1);
        calc_essence_free(((struct calc_essence_3op*)essence)->op2);
        calc_essence_free(((struct calc_essence_3op*)essence)->op3);
        break;
    default:
        print_error("Unknown type of essence: %d.", essence->type);
        BUG();
    }
    kfree(essence);
}

//advance to the next token in the string
static enum token_type
parse_data_next_token(struct parse_data* data)
{
    char ch;
    BUG_ON(data->current_token_type == token_type_error);
    BUG_ON(data->current_token_type == token_type_eof);
    // skip spaces
    while(isspace(data->expr[data->current_pos]))
        data->current_pos++;
    ch = data->expr[data->current_pos];
    debug("Next character is '%c'", ch);
    // whether token is name of constant or variable
    if(isalpha(ch) || (ch == '_'))
    {
        int name_len;
        const char* name = &data->expr[data->current_pos];
        // look for the end of name
        for(name_len = 1; isalnum(name[name_len]) || (name[name_len] == '_'); name_len++);
        // update position of current character
        data->current_pos = name + name_len - data->expr;
        if(!parse_data_search_const(data, name, name_len, &data->current_token_value))
        {
            data->current_token_type = token_type_constant;
        }
        else if(!parse_data_search_variable(data, name, name_len, &data->current_token_index))
        {
            data->current_token_type = token_type_variable;
        }
        else
        {
            print_error("Name %*s doesn't correspond to the name of constant or variable.",
                name_len, name);
            data->current_token_type = token_type_error;
        }
    }
    // whether token is number
    else if(isdigit(ch))
    {
        int number_len;
        const char* number = &data->expr[data->current_pos];
        //look for end of number
        for(number_len = 1; isdigit(number[number_len]); number_len++);
        // update position of current character
        data->current_pos = number + number_len - data->expr;
        if(!parse_number(number, number_len, &data->current_token_value))
        {
            data->current_token_type = token_type_value;
        }
        else
        {
            print_error("Cannot convert number %*s to string.",
                number_len, number);
            data->current_token_type = token_type_error;
        }
    }
    // whether token is special symbol(s), which mean operation or delimiter
    else switch(ch)
    {
    case '\0':
        data->current_token_type = token_type_eof;
        break;
#define ONE_SYMBOL_TOKEN(pure_type) data->current_token_type = token_type_##pure_type; data->current_pos++;
    case '(':
        ONE_SYMBOL_TOKEN(open_parenthesis);
        break;
    case ')':
        ONE_SYMBOL_TOKEN(close_parenthesis);
        break;
    case '~':
        ONE_SYMBOL_TOKEN(binary_not);
        break;
    case '*':
        ONE_SYMBOL_TOKEN(multiply);
        break;
    case '/':
        ONE_SYMBOL_TOKEN(divide);
        break;
    case '%':
        ONE_SYMBOL_TOKEN(rest);
        break;
    case '+':
        ONE_SYMBOL_TOKEN(plus);
        break;
    case '-':
        ONE_SYMBOL_TOKEN(minus);
        break;
    case '^':
        ONE_SYMBOL_TOKEN(binary_xor);
        break;
    case '?':
        ONE_SYMBOL_TOKEN(cond_first);
        break;
    case ':':
        ONE_SYMBOL_TOKEN(cond_second);
        break;
    case '=':
        ONE_SYMBOL_TOKEN(equal);
        break;
#undef ONE_SYMBOL_TOKEN
//determine token(1 of 2), which starts from current character
#define RESOLVE_TOKEN2(symbol, pure_type0, pure_type1) if(data->expr[++data->current_pos] == symbol)\
    {data->current_token_type = token_type_##pure_type1; data->current_pos++;}\
    else\
    {data->current_token_type = token_type_##pure_type0;}

    case '!':
        RESOLVE_TOKEN2('=', logical_not, inequal);
        break;
    case '|':
        RESOLVE_TOKEN2('|', binary_or, logical_or);
        break;
    case '&':
        RESOLVE_TOKEN2('&', binary_and, logical_and);
        break;
#undef RESOLVE_TOKEN2
//determine token(1 of 3), which start from current character
#define RESOLVE_TOKEN3(symbol_a, symbol_b, pure_type0, pure_type1_a, pure_type1_b) {char ch = data->expr[++data->current_pos];\
    if(ch == symbol_a) {data->current_token_type = token_type_##pure_type1_a; data->current_pos++;}\
    else if(ch == symbol_b) {data->current_token_type = token_type_##pure_type1_b; data->current_pos++;}\
    else {data->current_token_type = token_type_##pure_type0;}}

    case '>':
        RESOLVE_TOKEN3('>', '=', greater, right_shift, greater_equal);
        break;
    case '<':
        RESOLVE_TOKEN3('<', '=', less, left_shift, less_equal);
        break;
#undef RESOLVE_TOKEN3
    default:
        print_error("Unrecognized character '%c'.", ch);
        data->current_token_type = token_type_error;
    }
    return data->current_token_type;
}
// 'Main' function for parse string
static struct calc_essence* parse_data_parse(struct parse_data* data, int priority)
{
    struct calc_essence* result = NULL;
    // Operand
    switch(parse_data_next_token(data))
    {
        case token_type_error:
            break;//error was already been traced in parse_data_next_token()
        case token_type_eof:
            print_error0("End of file while operand expected");
            break;
        case token_type_value:
        case token_type_constant:
            result = calc_essence_val_create(data->current_token_value);
            if(result) parse_data_next_token(data);// advance to the next token
            break;
        case token_type_variable:
            result = calc_essence_var_create(data->current_token_value);
            if(result) parse_data_next_token(data);// advance to the next token
            break;
        case token_type_open_parenthesis:
            result = parse_data_parse(data, priority_min);
            if(!result) break;
            if(data->current_token_type != token_type_close_parenthesis)
            {
                print_error("Expected close parenthesis (')'), but token of type %d found.",
                    (int)data->current_token_type);
                // rollback result
                calc_essence_free(result);
                result = NULL;
                break;
            }
            parse_data_next_token(data);// advance to the next token
            break;
        //unary operations without left operand
#define UNARY_OP(essence_type_pure, priority_pure) {\
    struct calc_essence* op = parse_data_parse(data, priority_##priority_pure);\
    if(op == NULL) break;\
    result = calc_essence_1op_create(calc_essence_type_##essence_type_pure, op);\
    if(result == NULL) {calc_essence_free(op);}\
}
        case token_type_minus:
            UNARY_OP(unary_minus, unary_minus);
            break;
        case token_type_plus:
            UNARY_OP(unary_plus, unary_plus);
            break;
        case token_type_logical_not:
            UNARY_OP(logical_not, logical_not);
            break;
        case token_type_binary_not:
            UNARY_OP(binary_not, binary_not);
            break;
#undef UNARY_OP
        default:
            print_error("Expected operand, but token of type %d found.",
                (int)data->current_token_type);
    }

    if(result == NULL) return NULL;
    //Determine operation and update operand in cycle, until error, delemiter or operation with lower priority is encountered
    while(1)
    {
        switch(data->current_token_type)
        {
            case token_type_error:
                calc_essence_free(result);
                return NULL;// error was already been traced in parse_data_next_token()
            // delimiters
            case token_type_eof:
            case token_type_close_parenthesis:
            case token_type_cond_second:
                return result;
            // operations with two operands
#define TWO_OPERANDS_ESSENCE(essence_type_pure, left_priority_pure, right_priority_pure) \
if(priority > priority_##left_priority_pure) return result;\
{\
    struct calc_essence* op1, *op2;\
    debug("Evaluate second operand for essence of type %d...", (int)calc_essence_type_##essence_type_pure);\
    op2 = parse_data_parse(data, priority_##right_priority_pure);\
    if(op2 == NULL) {calc_essence_free(result); return NULL;}\
    op1 = result;\
    debug0("Create essence.");\
    result = calc_essence_2op_create(calc_essence_type_##essence_type_pure, op1, op2);\
    if(result == NULL) {calc_essence_free(op1); calc_essence_free(op2); return NULL;}\
}
            case token_type_multiply:
                TWO_OPERANDS_ESSENCE(multiply, multiply_left, multiply_right);
                break;
            case token_type_divide:
                TWO_OPERANDS_ESSENCE(divide, divide_left, divide_right);
                break;
            case token_type_rest:
                TWO_OPERANDS_ESSENCE(rest, rest_left, rest_right);
                break;
            case token_type_plus:
                TWO_OPERANDS_ESSENCE(plus, plus_left, plus_right);
                break;
            case token_type_minus:
                TWO_OPERANDS_ESSENCE(minus, minus_left, minus_right);
                break;
            case token_type_left_shift:
                TWO_OPERANDS_ESSENCE(left_shift, left_shift_left, left_shift_right);
                break;
            case token_type_right_shift:
                TWO_OPERANDS_ESSENCE(right_shift, right_shift_left, right_shift_right);
                break;
            case token_type_less:
                TWO_OPERANDS_ESSENCE(less, less_left, less_right);
                break;
            case token_type_greater:
                TWO_OPERANDS_ESSENCE(greater, greater_left, greater_right);
                break;
            case token_type_less_equal:
                TWO_OPERANDS_ESSENCE(less_equal, less_equal_left, less_equal_right);
                break;
            case token_type_greater_equal:
                TWO_OPERANDS_ESSENCE(greater_equal, greater_equal_left, greater_equal_right);
                break;
            case token_type_equal:
                TWO_OPERANDS_ESSENCE(equal, equal_left, equal_right);
                break;
            case token_type_inequal:
                TWO_OPERANDS_ESSENCE(inequal, inequal_left, inequal_right);
                break;
            case token_type_binary_or:
                TWO_OPERANDS_ESSENCE(binary_or, binary_or_left, binary_or_right);
                break;
            case token_type_binary_xor:
                TWO_OPERANDS_ESSENCE(binary_xor, binary_xor_left, binary_xor_right);
                break;
            case token_type_binary_and:
                TWO_OPERANDS_ESSENCE(binary_and, binary_and_left, binary_and_right);
                break;
            case token_type_logical_or:
                TWO_OPERANDS_ESSENCE(logical_or, logical_or_left, logical_or_right);
                break;
            case token_type_logical_and:
                TWO_OPERANDS_ESSENCE(logical_and, logical_and_left, logical_and_right);
                break;
#undef TWO_OPERANDS_ESSENCE
            //Only one three operand essence, so without macro
            case token_type_cond_first:
                if(priority > priority_cond_left) return result;
                {
                    struct calc_essence *op1, *op2, *op3;
                    debug0("Evaluate second operand for 'a ? b : c' operation...");\
                    op2 = parse_data_parse(data, priority_min);
                    if(op2 == NULL)
                    {
                        calc_essence_free(result);
                        return NULL;
                    }
                    if(data->current_token_type != token_type_cond_second)
                    {
                        print_error("Expected ':', but token of type %d encountered.",
                            (int)data->current_token_type);
                    }
                    debug0("Evaluate third operand for 'a ? b : c' operation...");\
                    op3 = parse_data_parse(data, priority_cond_right);
                    if(op3 == NULL)
                    {
                        calc_essence_free(op2);
                        calc_essence_free(result);
                        return NULL;
                    }
                    op1 = result;
                    debug0("Create essence.");
                    result = calc_essence_3op_create(calc_essence_type_cond, op1, op2, op3);
                    if(result == NULL)
                    {
                        calc_essence_free(op1);
                        calc_essence_free(op2);
                        calc_essence_free(op3);
                        return NULL;
                    }
                }
                break;
            //...
            default:
                print_error("Expected operation, but token of type %d found.",
                    (int)data->current_token_type);
                calc_essence_free(result);
                return NULL;
        }
    }
    return result;
}
// helpres for get additional attributes for token
int
parse_data_search_const(const struct parse_data* data,
    const char* constant_name, int constant_name_len, kedr_calc_int_t* value)
{
    int vector_index;
    for(vector_index = 0; vector_index < data->const_vec_n; vector_index++)
    {
        int constant_index;
        const struct kedr_calc_const_vec* vec = &data->const_vec[vector_index];
        for(constant_index = 0; constant_index < vec->n_elems; constant_index++)
        {
            const struct kedr_calc_const* const_def = &vec->elems[constant_index];
            if(strncmp(const_def->name, constant_name, constant_name_len) == 0)
            {
                *value = const_def->value;
                return 0;
            }
        }
    }
    return 1;
}
int
parse_data_search_variable(const struct parse_data* data,
    const char* variable_name, int variable_name_len, int* index)
{
    int var_index;
    for(var_index = 0; var_index < data->var_n; var_index++)
    {
        if(strncmp(data->var_names[var_index], variable_name, variable_name_len) == 0)
        {
             *index = var_index;
             return 0;
        }
    }
    return 1;
}
int
parse_number(const char* str, int str_len, kedr_calc_int_t* value)
{
    kedr_calc_int_t result;//temporary variable
    int i;
    BUG_ON(str_len == 0);
    
    for(result = str[0] - '0', i = 1; i < str_len; i++)
        result = result * 10 + (str[i] - '0');// currently do not verify overflow
    *value = result;
    return 0;
}