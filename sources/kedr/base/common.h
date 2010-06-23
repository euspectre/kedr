/*
 * This file contains common declarations provided by the controller for 
 * payload modules.
 */

#ifndef BASE_H_1739_INCLUDED
#define BASE_H_1739_INCLUDED

/*
 * The replacement table to be used by controller to actually instrument 
 * the target driver.
 */
struct kedr_repl_table
{
	/* array of original addresses of target functions 
	 * ("what to replace") */
	void** orig_addrs; 
	
	/* array of addresses of replacement functions 
	 * ("with what to replace") */
	void** repl_addrs; 
	
	/* number of elements to process in each of the two arrays above */
	unsigned int num_addrs;
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
	
	/* function replacement table */ 
	struct kedr_repl_table repl_table;
};

/* ================================================================ */
/* Public API                                                       */
/* ================================================================ */

/*
 * Use KEDR_MSG() instead of printk to output debug messages to the system
 * log.
 */
#undef KEDR_MSG /* just in case */
#ifdef KEDR_DEBUG
    # define KEDR_MSG(fmt, args...) printk(KERN_DEBUG "[KEDR] " fmt, ## args)
#else
    # define KEDR_MSG(fmt, args...) /* do nothing */
#endif

/* Register a payload module with the controller. 
 * 'payload' should provide all the data the controller needs to use this 
 * payload module.
 * This function returns 0 if successful, an error code otherwise.
 * 
 * This function is usually called in the init function of a payload module.
 * */
int 
kedr_payload_register(struct kedr_payload* payload);

/* Unregister a payload module, the controller will not use it any more.
 * 'payload' should be the same as passed to the corresponding call to
 * kedr_payload_register().
 * 
 * This function is usually called in the cleanup function of a payload 
 * module.
 * */
void 
kedr_payload_unregister(struct kedr_payload* payload);

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
 * */
int
kedr_target_module_in_init(void);
/* ================================================================ */
#endif /* BASE_H_1739_INCLUDED */
