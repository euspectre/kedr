#ifndef KEDR_FUNCTIONS_SUPPORT_INTERNAL_H
#define KEDR_FUNCTIONS_SUPPORT_INTERNAL_H

/*
 * Interface between KEDR payload processing(base functionality of KEDR)
 * and replacement functionality.
 */

#include <linux/module.h>

#include "kedr_base_internal.h"
#include "kedr_instrumentor_internal.h"

/*
 * Mark given function as used and prevent to unload support for it.
 * 
 * Several call of this function is allowed. In that case, all calls except
 * first will simply increase refcouning.
 * 
 * Return 0 on success.
 * On error return negative error code.
 */
int kedr_functions_support_function_use(void* function);
/*
 * Mark given function as unused and allow to unload support for it.
 * 
 * If kedr_functions_support_function_use was called more than once, than
 * this function should be called same times.
 * 
 * Return 0 on success.
 * On error return negative error code.
 */
int kedr_functions_support_function_unuse(void* function);

/*
 * Accept array of functions which should be intercepted and
 * return array of replacements for this functions, which
 * implement given interceptions.
 * 
 * After successfull call of this functions and until call of
 * kedr_function_support_release()
 * one cannot register new functions support.
 * 
 * Returning array will be freed at kedr_function_support_release() call.
 * 
 * On error return ERR_PTR().
 * */
const struct kedr_instrumentor_replace_pair*
kedr_functions_support_prepare(const struct kedr_base_interception_info* info);

/*
 * Release support for functions given at
 * kedr_functions_support_prepare call.
 */

void kedr_functions_support_release(void);

int kedr_functions_support_init(void);
void kedr_functions_support_destroy(void);

#endif /* KEDR_FUNCTIONS_SUPPORT_INTERNAL_H */