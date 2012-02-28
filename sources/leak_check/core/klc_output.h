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
kedr_lc_output_create(struct module *target);

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

/* Types of information that can be output.
 * The point is, different types of information can be output to different
 * places or distinguished in some other way. */
enum kedr_lc_output_type {
	/* possible leaks */
	KLC_UNFREED_ALLOC,
	
	/* bad frees */
	KLC_BAD_FREE,
	
	/* other info: parameters of the target module, totals, ... */
	KLC_OTHER
};

/* Outputs a string pointed to by 's' taking type of this information into.
 * account ('output_type').
 * The implementation defines where the string will be output and how 
 * different kinds of information will be distinguished.
 * This function is a basic block for the functions that output particular
 * data structures.
 *
 * A newline will be added at the end automatically.
 *
 * This function cannot be used in atomic context. */
void
kedr_lc_print_string(struct kedr_lc_output *output, 
	enum kedr_lc_output_type output_type, const char *s);

/* Outputs first 'num_entries' elements of 'stack_entries' array as a stack
 * trace. 
 * 
 * This function cannot be used in atomic context. */
void
kedr_lc_print_stack_trace(struct kedr_lc_output *output, 
	enum kedr_lc_output_type output_type, 
	unsigned long *stack_entries, unsigned int num_entries);

/* Output information about the target module.
 *
 * This function cannot be used in atomic context. */
void
kedr_lc_print_target_info(struct kedr_lc_output *output);

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

/* Output statistics about the analysis session of the target module:
 * total number of resource allocations, potential leaks and spurious 
 * ("unallocated") frees.
 * 
 * Should be called from on_target_unload() handler. */
void
kedr_lc_print_totals(struct kedr_lc_output *output, u64 total_allocs, 
	u64 total_leaks, u64 total_bad_frees);

#endif /* KLC_OUTPUT_H_1810_INCLUDED */
