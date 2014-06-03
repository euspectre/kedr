#ifndef KEDR_BASE_INTERNAL_H
#define KEDR_BASE_INTERNAL_H

/*
 * Interface between KEDR payload processing(base functionality of KEDR)
 * and replacement functionality.
 */

#include <linux/module.h>

/*
 * Define what should be done when original function will be called.
 */
struct kedr_base_interception_info
{
    void* orig;
    
    // NULL-terminated array of pre-functions(may be NULL)
    void** pre;
    // NULL-terminated array of post-functions(may be NULL)
    void** post;
    // replacement function or NULL.
    void* replace;
};

/*
 * Fix all payloads and return array of functions with information
 * how them should be intercepted.
 * 
 * Last element in the array should contain NULL in 'orig' field.
 * 
 * On error, return ERR_PTR.
 * 
 * Returning array is freed by the callee
 * at kedr_target_unload_callback() call.
 */
const struct kedr_base_interception_info*
kedr_base_target_load_callback(struct module* m);

/*
 * Make all payloads available to unload.
 */
void kedr_base_target_unload_callback(struct module* m);

/*
 * Callback functions which are used by kedr_base component.
 */
struct kedr_base_operations
{
    /* called when registered payload, which intercept this function. */
    int (*function_use)(struct kedr_base_operations* ops, void* function);
    /* called when unregister payload which intercept this function. */
    int (*function_unuse)(struct kedr_base_operations* ops, void* function);
};

/*
 * Initialize and destroy KEDR base functionality.
 */
int kedr_base_init(struct kedr_base_operations* ops);
void kedr_base_destroy(void);

#endif /* KEDR_BASE_INTERNAL_H */
