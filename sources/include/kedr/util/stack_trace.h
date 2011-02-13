/* stack_trace.h 
 * Stack trace helpers for payload modules in KEDR.
 */

#ifndef KEDR_STACK_TRACE_H_1637_INCLUDED
#define KEDR_STACK_TRACE_H_1637_INCLUDED

/* Maximum number of stack frames above the call point that can be saved.
 */
#define KEDR_MAX_FRAMES 16

/* Maxiumum number of stack frames 'below' the call point + 1 for the 
 * latter. These are usually from the implementation of save_stack_trace()
 * + one frame for kedr_save_stack_impl().
 */
#define KEDR_LOWER_FRAMES 6

/* Maximum number of frames (entries) to store internally. 
 * Technically, it is (KEDR_MAX_FRAMES + KEDR_LOWER_FRAMES) rounded up to
 * the next multiple of 16.
 * The additional entries may help handle the case when there are more than
 * KEDR_LOWER_FRAMES lower frames.
 * 
 * [NB] To round up a nonnegative number x to a multiple of N, use
 * (x + N - 1) & ~(N - 1) 
 */
#define KEDR_NUM_FRAMES_INTERNAL \
    ((KEDR_MAX_FRAMES + KEDR_LOWER_FRAMES + 15) & ~15)

/* [NB] This function is not intended to be used directly. 
 * Use kedr_save_stack_trace() macro with the corresponding parameters 
 * instead
 *
 * kedr_save_stack_trace_impl() saves up to 'max_entries' stack trace
 * entries in the 'entries' array provided by the caller. 
 * After the call, *nr_entries will contain the number of entries actually
 * saved. 
 *
 * The difference from save_stack_trace() is that only the entries from 
 * above the call point will be saved. That is, the first entry will
 * correspond to the caller of the function that called 
 * kedr_save_stack_trace_impl(), etc. We are not often interested in the 
 * entries corresponding to the implementation of save_stack_trace(),
 * that is why the 'lower' entries will be omitted.
 *
 * This function is intended to be used in the replacement functions, so
 * it is not that important that it will not store the entry corresponding 
 * to its direct caller.
 * The question that a function "asks" by calling 
 * kedr_save_stack_trace_impl() is "Who called me?"
 *
 * 'max_entries' should not exceed KEDR_MAX_FRAMES.
 *
 * 'entries' should have space for at least 'max_entries' elements.
 *
 * 'first_entry' is the first stack entry we are interested in. It is 
 * usually obtained with __builtin_return_address(0). 
 * [NB] If the results of save_stack_trace() are not reliable (e.g. if that
 * function is a no-op), 'entries[0]' will contain the value of 
 * 'first_entry' and '*nr_entries' will be 1.
 */
void
kedr_save_stack_trace_impl(unsigned long *entries, unsigned int max_entries,
    unsigned int *nr_entries,
    unsigned long first_entry);

/* A helper macro to obtain the call stack (this is a macro because it
 * allows using __builtin_return_address(0) to obtain the first stack 
 * entry).
 * For the description of parameters, see kedr_save_stack_trace_impl() 
 * above.
 */
#define kedr_save_stack_trace(entries_, max_entries_, nr_entries_) \
    kedr_save_stack_trace_impl(entries_, max_entries_, \
        nr_entries_, \
        (unsigned long)__builtin_return_address(0))

#endif /* KEDR_STACK_TRACE_H_1637_INCLUDED */
