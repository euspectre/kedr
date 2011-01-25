/* mbi_ops.h
 * Operations with the storage for klc_memblock_info structures:
 * addition to and removal from the storage, searching, etc.
 * 
 * The goal is for the analysis system to use the interface to this storage
 * that does not heavily depend on the underlying data structures.
 */

#ifndef MBI_OPS_H_1723_INCLUDED
#define MBI_OPS_H_1723_INCLUDED

#include "memblock_info.h"

/* Initializes the storage for klc_memblock_info structures corresponding
 * to memory allocation and deallocation events.
 * 
 * This function should better be called once during the initialization of 
 * the module.
 */
void 
klc_init_mbi_storage(void);

/* Adds the structure pointed to by 'alloc_info' to the list of 
 * "allocation events".
 *
 * Use klc_add_alloc() macro rather than this function in the replacement
 * functions.
 */
void
klc_add_alloc_impl(struct klc_memblock_info *alloc_info);

/* Adds the structure pointed to by 'dealloc_info' to the list of 
 * "suspicious deallocation events".
 *
 * Use klc_add_bad_free() macro rather than this function in the replacement
 * functions.
 */
void
klc_add_bad_free_impl(struct klc_memblock_info *dealloc_info);

/* Helpers to create klc_memblock_info structures and add them to 
 * the storage in one step.
 * Note that in case of low memory, only a message will be output to
 * the system log, the error will not be reported in any other way.
 */
#define klc_add_alloc(block_, size_, max_stack_depth_)              \
{                                                                   \
    struct klc_memblock_info *mbi;                                  \
    mbi = klc_alloc_info_create((block_), (size_),                  \
        (max_stack_depth_));                                        \
    if (mbi == NULL) {                                              \
        printk(KERN_ERR "[kedr_leak_check] klc_add_alloc: "         \
        "not enough memory to create 'struct klc_memblock_info'\n");\
    } else {                                                        \
        klc_add_alloc_impl(mbi);                                    \
    }                                                               \
}

#define klc_add_bad_free(block_, max_stack_depth_)                  \
{                                                                   \
    struct klc_memblock_info *mbi;                                  \
    mbi = klc_dealloc_info_create((block_), (max_stack_depth_));    \
    if (mbi == NULL) {                                              \
        printk(KERN_ERR "[kedr_leak_check] klc_add_bad_free: "      \
        "not enough memory to create 'struct klc_memblock_info'\n");\
    } else {                                                        \
        klc_add_bad_free_impl(mbi);                                 \
    }                                                               \
}

/* This function is usually called from deallocation handlers.
 * It looks for the item in the storage corresponding to the allocation
 * event with 'block' field equal to 'block'.
 * If it is found, i.e. if a matching allocation event is found, 
 * the function removes the item from the storage, deletes the item itself 
 * (no need to store it any longer) and returns nonzero.
 * Otherwise, the function returns 0 and leaves the storage unchanged.
 *
 * 'block' must not be NULL.
 */
int
klc_find_and_remove_alloc(const void *block);

/* Outputs the information about the allocation events currently present 
 * in the storage, detetes the corresponding entries from the storage and
 * then destroys them.
 * The storage should be empty as a result.
 * 
 * As the output routines this function uses cannot be called in atomic 
 * context, this function cannot be called in atomic context either.
 * The caller must also ensure it is not called when other operations
 * with the storage may happen.
 * 
 * This is normally not a problem because this function is intended to
 * be called from on_target_unload() handler. It is guaranteed that 
 * the replacement functions have already done their work by then, so no
 * other operation with the storage can be active at the moment.
 */
void
klc_flush_allocs(void);

/* This function does the same as klc_flush_allocs() but for the storage
 * of spurious deallocation events.
 */
void
klc_flush_deallocs(void);

/* Outputs and then resets allocation statistics collected so far: 
 * total number of allocations, possible leaks, etc. 
 * 
 * Should be called from on_target_unload() handler.
 */
void
klc_flush_stats(void);

#endif /* MBI_OPS_H_1723_INCLUDED */
