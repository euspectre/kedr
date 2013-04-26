/* klc_output.h - helpers for data output.
 * This provides the additional abstraction that allows to output data from 
 * the payload module. */

#ifndef KLC_OUTPUT_H_1810_INCLUDED
#define KLC_OUTPUT_H_1810_INCLUDED

/* [NB] The caller must ensure that no output via kedr_lc_print_* functions
 * takes place when kedr_lc_output_*() functions are running. */

struct module;

/* The "output objects" are the instances of struct kedr_lc_output. For each
 * LeakCheck object, an output object should be created and associated with 
 * it. */
struct kedr_lc_output;
struct kedr_leak_check;
 
/* Initializes the output subsystem as a whole. This function should usually
 * be called from the module's init function. kedr_lc_output_init() should 
 * be called before any other kedr_lc_output_* and kedr_lc_print_* 
 * functions. 
 * 
 * Returns 0 on success, -errno on failure. */
int
kedr_lc_output_init(void);

/* Finalizes the output subsystem as a whole. This function should usually
 * be called from the module's cleanup function, after all output objects
 * have been destroyed. */
void 
kedr_lc_output_fini(void);

/* Creates and initializes the output object for the data to be obtained 
 * during the analysis of the target. 
 * Returns the pointer to the object on success, ERR_PTR(-errno) on failure.
 * The function never returns NULL. 
 * Cannot be called from atomic context.*/
struct kedr_lc_output *
kedr_lc_output_create(struct module *target, struct kedr_leak_check *lc);

/* Performs cleaning up in the given output object ('output') and destroys 
 * the object. Does nothing if 'output' is NULL.
 * This function should usually be called from the module's cleanup
 * function. If the output subsystem created files in debugfs, it is unsafe
 * to remove them before the module's cleanup anyway. */
void
kedr_lc_output_destroy(struct kedr_lc_output *output);

/* Clears the output data. For example, it may clear the contents of the 
 * files that stored information for the previous analysis session for 
 * the target module.
 *
 * This function should usually be called from on_target_load() handler
 * or the like to clear old data. */
void
kedr_lc_output_clear(struct kedr_lc_output *output);

/* Output information about the target module.
 *
 * This function cannot be used in atomic context. */
void
kedr_lc_print_target_info(struct kedr_lc_output *output, 
	struct module *target, void *init_area, void *core_area);

/* Helpers to output kedr_lc_resource_info structures corresponding to 
 * suspicious resource allocation and deallocation events.
 *
 * Cannot be used in atomic context. */
void 
kedr_lc_print_alloc_info(struct kedr_lc_output *output, 
	struct kedr_lc_resource_info *info, u64 similar_allocs);

void 
kedr_lc_print_dealloc_info(struct kedr_lc_output *output, 
	struct kedr_lc_resource_info *info, u64 similar_deallocs);

/* Output a note that only 'reported' of 'total' bad free events have
 * been reported. */
void 
kedr_lc_print_dealloc_note(struct kedr_lc_output *output, 
	u64 reported, u64 total);

/* Output statistics about the analysis session of the target module:
 * total number of resource allocations, potential leaks and spurious 
 * ("unallocated") frees.
 * 
 * Should be called from on_target_unload() handler. */
void
kedr_lc_print_totals(struct kedr_lc_output *output, u64 total_allocs, 
	u64 total_leaks, u64 total_bad_frees);

#endif /* KLC_OUTPUT_H_1810_INCLUDED */
