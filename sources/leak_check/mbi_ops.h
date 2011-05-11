/* mbi_ops.h
 * Operations with the storage for klc_memblock_info structures:
 * addition to and removal from the storage, searching, etc.
 * 
 * The goal is for the analysis system to use the interface to this storage
 * that does not heavily depend on the underlying data structures. */

#ifndef MBI_OPS_H_1723_INCLUDED
#define MBI_OPS_H_1723_INCLUDED

/* Initializes the storage for klc_memblock_info structures corresponding
 * to memory allocation and deallocation events.
 * 
 * This function should better be called once during the initialization of 
 * the module. */
void 
klc_init_mbi_storage(void);

/* Helpers to create klc_memblock_info structures and request them to 
 * be processed (that is, these helpers implement the "top half", the 
 * ideas are similar to those used in interrupt handling).
 * 
 * Note that in case of low memory, only a message will be output to
 * the system log, the error will not be reported in any other way. 
 * 
 * 'caller_address' is the address of the instruction immediately 
 * following the call to the allocation/deallocation function. That is,
 * it is the return address of the latter. 
 * This parameter is provided by the pre- and post- handlers in the payload
 * modules. */
void
klc_handle_alloc(const void *block, size_t size, 
	unsigned int max_stack_depth,
	const void *caller_address);
void
klc_handle_free(const void *block, unsigned int max_stack_depth,
	const void *caller_address);

/* Performs initialization tasks deferred until the target has been loaded:
 * creates the workqueue, etc. If it fails to initialize some facility, the
 * latter will be disabled and a message will be output in the system log.
 * The execution should continue (naturally, without collecting and 
 * processing some of the data, etc.).
 * 
 * The function should be called from on_target_load(). */
void
klc_handle_target_load(struct module *target_module);

/* The function waits until all pending operations processing allocation
 * and deallocation events are complete. Then it outputs the information 
 * about the allocation events currently present in the storage, deletes 
 * the corresponding entries from the storage and then destroys them.
 * The storage should be empty as a result. After that, the function does
 * the same for the storage of spurious deallocation events.
 * 
 * Finally, it outputs and then resets allocation statistics collected so 
 * up to the moment: total number of allocations, possible leaks, etc. 
 *
 * Should be called from on_target_unload() handler (it is guaranteed that 
 * the replacement functions have already done their work by then). */
void
klc_handle_target_unload(struct module *);

#endif /* MBI_OPS_H_1723_INCLUDED */
