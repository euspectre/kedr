/*
 * This file contains common declarations to be used by the controller
 * and payload modules.
 */

#ifndef KEDR_H
#define KEDR_H

#include <linux/module.h> /* struct module */

/**********************************************************************
 * Public API                                                        
 **********************************************************************/


/*
 * Information about original function call,
 * which is passed to the functions, which intended to call
 * before, after or instead it.
 */
struct kedr_function_call_info
{
	// Returning address from the original function.
	void* return_address;
};

/*
 * Pair of function which should be replaced and function,
 * which should replace it.
 * 
 * If original function has signature
 * ret_type (*)(arg_type1,..., arg_typeN)
 * 
 * then replacement one should have signature
 * 
 * ret_type (*)(arg_type1,..., arg_typeN, kedr_function_call_info*).
 */
struct kedr_replace_pair
{
	// Address of the function to replace
	void* orig;
	// Address of the function which should be executed instead of original.
	void* replace;
};

/*
 * Pair of function which should be intercepet and function,
 * which should be called before it.
 * 
 * If original function has signature
 * ret_type (*)(arg_type1,..., arg_typeN)
 * 
 * then another one should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, kedr_function_call_info*).
 */
struct kedr_pre_pair
{
	// Address of the function to intercept
	void* orig;
	// Address of the function which should be executed before original.
	void* pre;
};

/*
 * Pair of function which should be intercepet and function,
 * which should be called after it.
 *
 * If original function has signature('ret_type' is not 'void')
 * ret_type (*)(arg_type1,..., arg_typeN)
 * 
 * then another one should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, ret_type, kedr_function_call_info*).
 * 
 * If original function has signature
 * void (*)(arg_type1,..., arg_typeN)
 * 
 * then another one should have signature
 * 
 * void (*)(arg_type1,..., arg_typeN, kedr_function_call_info*).
 */

struct kedr_post_pair
{
	// Address of the function to intercept
	void* orig;
	// Address of the function which should be executed before original.
	void* post;
};


/* This structure contains everything KEDR needs to properly 
 * operate on a payload module (the module that actually provides the 
 * replacement functions, etc.).
 * 
 * KEDR does not need to know the internals of a particular payload module.
 * */
struct kedr_payload
{
	/* payload module itself */
	struct module* mod; 
	
	/* 
	 * Array of functions for replace.
	 * 
	 * Last element in that array should contain NULL
	 * as the address of the original function.
	 * 
	 * May be NULL.
	 */
	struct kedr_replace_pair* replace_pairs;
    
	/* 
	 * Array of functions for perform some actions before them.
	 * 
	 * Last element in that array should contain NULL
	 * as the address of the original function.
	 * 
	 * May be NULL.
	 */
	struct kedr_pre_pair* pre_pairs;

	/* 
	 * Array of functions for perform some actions after them.
	 * 
	 * Last element in that array should contain NULL
	 * as the address of the original function.
	 * 
	 * May be NULL.
	 */
	struct kedr_post_pair* post_pairs;


    /* If not NULL, these callbacks are called after the target module is
     * loaded (but before it begins its initialization) and, respectively,
     * when the target module has done cleaning up and is about to unload.
     * The callbacks are passed the pointer to the target module as 
     * an argument. 
     * 
     * If a callback is NULL, it is ignored.
     */
    void (*target_load_callback)(struct module*);
    void (*target_unload_callback)(struct module*);
};

/* Registers a payload module with the controller. 
 * 'payload' should provide all the data the controller needs to use this 
 * payload module.
 * This function returns 0 if successful, an error code otherwise.
 * 
 * This function is usually called in the init function of a payload module.
 * */
int 
kedr_payload_register(struct kedr_payload *payload);

/* Unregisters a payload module, the controller will not use it any more.
 * 'payload' should be the same as passed to the corresponding call to
 * kedr_payload_register().
 * 
 * This function is usually called in the cleanup function of a payload 
 * module.
 * */
void 
kedr_payload_unregister(struct kedr_payload *payload);

/* Returns nonzero if a target module is currently loaded and it executes 
 * its init function at the moment, 0 otherwise (0 is returned even if there
 * is no target module loaded at the moment).
 * 
 * In fact, the function just checks whether the target module has already
 * dropped its ".init.*" sections (which the modules do after they have 
 * completed their initialization). Therefore the function will always 
 * return 0 if the init function was not marked as "__init" in the target 
 * module. This should not be a big problem.
 * 
 * This function can be useful to implement particular fault simulation 
 * scenarios (like "fail everything after init"), etc.
 * 
 * Note however that there is a chance that the target module will complete
 * its initialization after kedr_target_module_in_init() has determined that
 * the target is in init but before the return value of 
 * kedr_target_module_in_init() is used. It is up to the user of the target
 * module to ensure that no request is made to the module until its 
 * initialization is properly handled by the tests.
 *
 * It is allowed to call this function from atomic context.
 * */
int
kedr_target_module_in_init(void);


/*
 * Use KEDR_MSG() instead of printk to output debug messages to the system
 * log.
 */
#undef KEDR_MSG /* just in case */
#ifdef KEDR_DEBUG
    # define KEDR_MSG(fmt, args...) printk(KERN_DEBUG "[KEDR] " fmt, ## args)
#else
    # define KEDR_MSG(fmt, args...) do { } while(0) /* do nothing */
#endif

/**********************************************************************
 * API for module, which add implementation of intermediate functions.
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
int kedr_functions_support_register(struct kedr_functions_support* supp);
int kedr_functions_support_unregister(struct kedr_functions_support* supp);

#endif /* KEDR_H */
