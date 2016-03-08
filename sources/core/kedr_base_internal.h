#ifndef KEDR_BASE_INTERNAL_H
#define KEDR_BASE_INTERNAL_H

/*
 * Interface between KEDR payload processing(base functionality of KEDR)
 * and replacement functionality.
 */

#include <linux/module.h>

/* 
 * When register payload, this callback is called for every function
 * which payload require to intercept.
 * 
 * Return 0 on success, -err if interception of that function is
 * prohibited.
 * 
 * Should be defined elsewhere.
 */
extern int kedr_functions_support_function_use(void* function);
/* 
 * When deregister payload, this callback is called for every function
 * which payload required to intercept.
 * 
 * Should be defined elsewhere.
 */
extern void kedr_functions_support_function_unuse(void* function);

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
 * Last element in the array contains NULL in 'orig' field.
 * 
 * On error, return ERR_PTR.
 * 
 * Returning array is freed by the callee
 * at kedr_base_session_end() call.
 */
const struct kedr_base_interception_info*
kedr_base_session_start(void);

/*
 * Make all payloads available to unload.
 */
void kedr_base_session_stop(void);


/* Inform payloads about target module being loaded/unloaded. */
void kedr_base_target_load(struct module* m);
void kedr_base_target_unload(struct module* m);

/*
 * Initialize and destroy KEDR base functionality.
 */
int kedr_base_init(void);
void kedr_base_destroy(void);

/*
 * Allow registration only of those payloads, which supports several targets.
 * 
 * If any of currently registered payload is single-target, return
 * negative error code and print message into kernel log.
 * 
 * NOTE: Function is allowed to be called even before kedr_base_init().
 */
int force_several_targets(void);

/* 
 * Allow registration of all payloads, even single-target ones.
 * 
 * NOTE: Function is allowed to be called even before kedr_base_init().
 */
void unforce_several_targets(void);

#endif /* KEDR_BASE_INTERNAL_H */
