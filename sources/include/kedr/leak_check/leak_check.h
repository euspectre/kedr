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
 * 2. Do not call the functions listed below with NULL or ZERO_SIZE_PTR
 * as 'addr'. */
/* ====================================================================== */

/* Call this function to inform LeakCheck core that the given kernel module
 * ('mod') has allocated a resource (e.g. memory block, some object, etc.) 
 * at the address 'addr' in memory. The size of the resource is 'size'.
 *
 * [NB] If the resource is a memory block, 'size' should be the size of this
 * block. For other types of resources, it is also recommended to provide 
 * a meaningful value of 'size'. In some cases, the size of the structure 
 * corresponding to the resource could be convenient to use here.
 * 
 * If the size cannot be obtained, pass 0 as 'size'. This will be 
 * interpreted as "unknown size" by LeakCheck
 *
 * 'size' must not be equal to (size_t)(-1), this value is reserved.
 * 
 * This function should be called AFTER the resource has actually been 
 * successfully allocated. */
void
kedr_lc_handle_alloc(struct module *mod, const void *addr, size_t size, 
	const void *caller_address);

/* Call this function to inform LeakCheck core that the given kernel module 
 * ('mod') has freed (released) the resource that was located at the given 
 * address in memory ('addr'). 
 * 
 * This function should be called BEFORE the resource is actually freed.
 * This is because LeakCheck assumes that the calls to its API happen in 
 * exactly the same order as the corresponding allocation and deallocation
 * calls in the code under analysis.
 * 
 * If kedr_lc_handle_free() was called after the deallocation, some other 
 * part of the code under analysis could get in between the actual 
 * deallocation and that call and allocate the resource. The chances are,
 * the new resource will have the same address as the old one. As a 
 * result, a call to kedr_lc_handle_alloc() could also occur before the 
 * call to kedr_lc_handle_free() with the same 'addr' value, which could
 * make a mess. */
void
kedr_lc_handle_free(struct module *mod, const void *addr, 
	const void *caller_address);

#endif /* LEAK_CHECK_H_1042_INCLUDED */
