#ifndef KEDR_BASE_INTERNAL_H
#define KEDR_BASE_INTERNAL_H

/*
 * Interface between KEDR payload processing(base functionality of KEDR)
 * and replacement functionality.
 */

#include <linux/module.h>

/*
 * Define pair original function -> real replacement function
 * Both functions should have same signature.
 */
struct kedr_replace_real_pair
{
    void* orig;
    void* repl;
};

/*
 * Fix all payloads and return array of functions to replace.
 * 
 * Last element in the array should contain NULL in 'orig' field.
 * 
 * On error, return NULL.
 *
 * TODO: It make sence to return ERR_PTR on error.
 * 
 * Returning array is freed by the callee
 * at kedr_target_unload_callback() call.
 */
struct kedr_replace_real_pair*
kedr_base_target_load_callback(struct module* m);

/*
 * Make all payloads available to unload.
 */
void kedr_base_target_unload_callback(struct module* m);

/*
 * Initialize and destroy KEDR base functionality.
 */
int kedr_base_init(void);
void kedr_base_destroy(void);

#endif /* KEDR_INTERNAL_H */