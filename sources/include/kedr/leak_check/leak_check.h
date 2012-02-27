/* leak_check.h - public API provided by LeakCheck core. */

#ifndef LEAK_CHECK_H_1042_INCLUDED
#define LEAK_CHECK_H_1042_INCLUDED

struct module;

/* Notes:
 *
 * 1. In the functions below, 'caller_address' is the return address of the 
 * call to the corresponding allocation or deallocation function. That is 
 * the address of the machine instruction immediately following the call
 * (x86-specific).
 * The pre- and post-handlers in KEDR payloads have 'caller_address' 
 * variable for this purpose, just pass it here. If LeakCheck API is used
 * in some other component rather than a KEDR payload module, it is the job
 * of that component to properly determine the value to be passed as 
 * 'caller_address'. 
 * 
 * 2. Do not call the functions listed below for NULLs and ZERO_SIZE_PTRs 
 * as "block addresses". */
/* ====================================================================== */

/* Call this function to inform LeakCheck core that the given kernel module
 * ('mod') has freed a block of memory starting from the address 'block'. 
 * 
 * This function should be called AFTER the memory has actually been 
 * successfully allocated. */
void
kedr_lc_handle_alloc(struct module *mod, const void *block, size_t size, 
	const void *caller_address);

/* Call this function to inform LeakCheck core that the given kernel module 
 * ('mod') has freed a block of memory starting from the address 'block'. 
 * 
 * This function should be called BEFORE the memory is actually deallocated.
 * This is because LeakCheck assumes that the calls to its API happen in 
 * exactly the same order as the corresponding allocation and deallocation
 * calls in the code under analysis.
 * 
 * If kedr_lc_handle_free() was called after the deallocation, some other 
 * part of the code under analysis could get in between the actual 
 * deallocation and that call and allocate memory. The chances are, the 
 * system could give then the memory that has just been deallocated. As a 
 * result, a call to kedr_lc_handle_alloc() could also occur before the 
 * call to kedr_lc_handle_free() with the same block address. */
void
kedr_lc_handle_free(struct module *mod, const void *block, 
	const void *caller_address);

#endif /* LEAK_CHECK_H_1042_INCLUDED */
