/*
 * This file contains common declarations to be used by the controller
 * and payload modules.
 */

#ifndef COMMON_H_1739_INCLUDED
#define COMMON_H_1739_INCLUDED

/**********************************************************************
 * Public API                                                        
 **********************************************************************/
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

/* Registers a payload module with the controller. 
 * 'payload' should provide all the data the controller needs to use this 
 * payload module.
 * This function returns 0 if successful, an error code otherwise.
 * 
 * This function is usually called in the init function of a payload module.
 * */
int 
kedr_payload_register(struct kedr_payload* payload);

/* Unregisters a payload module, the controller will not use it any more.
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
 *
 * It is allowed to call this function from atomic context.
 * */
int
kedr_target_module_in_init(void);

/********************************************************************** 
 * Interface for the controller
 * DO NOT use it for payloads and custom modules.
 * This interface should be used only for implementation of KEDR itself,
 * hence 'kedr_impl' prefix in the names.
 **********************************************************************/

/* 
 * This structure contains the pointers to the functions that will 
 * actually be delegated the job when some of public functions are called.
 *
 * The goal is, the API for payload modules should be provided by "base"
 * only (the only component that is guaranteed to be loaded before any 
 * payload module). 
 * However, the functions like kedr_target_module_in_init() require data
 * owned by controller and the controller is to be loaded after payload 
 * modules. The functions like that are meaningful only when the controller 
 * is loaded but the controller cannot simply export these as it is loaded 
 * last.
 *
 * Ideally, controller should require no knowledge about the payload 
 * modules. Its main responsibility is to find the target module (or wait 
 * for it to be loaded) and instrument it.
 *
 * To resolve the issue, the delegates emerge.
 * Some of the public functions provided by the base are little more than 
 * wrappers around the "delegate" functions provided by controller when it 
 * registers itself with the base.
 */
struct kedr_impl_delegates
{
	/* This delegate must not sleep/reschedule as it can be called 
	 * from an atomic context. It may use spinlocks however.
	 */
	int (*target_module_in_init)(void);
};

/* This structure represents a controller */
struct kedr_impl_controller
{
	/* kernel module of the controller */
	struct module* mod; 
	
	/* delegate functions */
	struct kedr_impl_delegates delegates;
};

/*
 * Registers the controller module with the base.
 * Only one controller can be registered at a time.
 * The function returns 0 if successful, an error code otherwise.
 */
int 
kedr_impl_controller_register(struct kedr_impl_controller* controller);

/* 
 * Unregisters the controller (should be called in the controller's cleanup
 * function).
 */
void
kedr_impl_controller_unregister(struct kedr_impl_controller* controller);

/*
 * This function is called by the controller to inform the base that 
 * a target module has been loaded into memory and is about to be 
 * instrumented.
 * The base can use this to lock the payload modules in memory, etc.
 * The base also fills the combined replacement table (*ptable).
 * In case of failure, the contents of the table are undefined.
 *
 * The replacement table is created from the data provided by all 
 * registered payloads. The contents of the table are owned by the base and 
 * must not be modified by the caller.
 *
 * The function returns 0 if successful, an error code otherwise.
 */
int
kedr_impl_on_target_load(struct kedr_repl_table* ptable);
	
/*
 * This function is called by the controller to inform the base that 
 * a target module has been unloaded.
 *
 * The function returns 0 if successful or an error code in case of failure.
 */
int
kedr_impl_on_target_unload(void);

/**********************************************************************/
#endif /* COMMON_H_1739_INCLUDED */
