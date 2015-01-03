/*
 * This file contains common declarations to be used by
 * functions_support modules.
 */

#ifndef KEDR_FUNCTIONS_SUPPORT_H
#define KEDR_FUNCTIONS_SUPPORT_H

#include <linux/module.h> /* struct module */

/**********************************************************************
 * Public API                                                        
 **********************************************************************/

/*
 * Information for intermediate function.
 */
struct kedr_intermediate_info
{
    //NULL-terminated array of pre-functions
    void** pre;
    //NULL-terminated array of post-functions
    void** post;
    // replacement function or NULL.
    void* replace;
};

/*
 * Information about one intermediate replacement function implementation.
 * 
 * This implementation should provide that
 * all functions in 'pre' and 'post' arrays
 * is called in the correct order with respect to 'orig' function.
 * Also, if 'replace' functions is set, it should be called instead
 * of 'orig' function.
 */
struct kedr_intermediate_impl
{
    void* orig;
    void* intermediate;
    /*
     *  Next field will be filled only before target module is loaded,
     * and makes a sence only during target session.
     */
    struct kedr_intermediate_info* info;
};

struct kedr_functions_support
{
    /*
     * Module which will be prevented to unload
     * while this support is used.
     * 
     * If module itself use this support(e.g., define payload),
     * field should be set to NULL.
     * (otherwise one will unable to unload this module at all).
     */
    struct module* mod;

    /*
     * Array of intermediate functions implementations.
     * 
     * Last element of the array should hold NULL in 'orig'.
     */
    struct kedr_intermediate_impl* intermediate_impl;
};
/*
 * Register kedr support for some functions set.
 * 
 * Functions sets from different registrations may intercept
 * with each other. Which intermediate representation will be used
 * in this case is implementation-defined.
 */
int kedr_functions_support_register(struct kedr_functions_support* functions_support);
int kedr_functions_support_unregister(struct kedr_functions_support* functions_support);

#endif /* KEDR_H */
