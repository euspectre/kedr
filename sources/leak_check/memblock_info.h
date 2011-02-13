/* memblock_info.h 
 * Definition of the structure containing information about allocated 
 * or freed memory block: the pointer to that block, stack trace, etc.
 *
 * Helper macros to deal with such structures are also defined here.
 */

#ifndef MEMBLOCK_INFO_H_1734_INCLUDED
#define MEMBLOCK_INFO_H_1734_INCLUDED

#include <linux/list.h>
#include <linux/slab.h>

#include <kedr/util/stack_trace.h>

/* This structure contains data about a block of memory:
 * the pointer to that block ('block') and a portion of the call stack for
 * the appropriate call to an allocation or deallocation function 
 * ('stack_entries' array containing 'num_entries' meaningful elements).
 * 
 * The instances of this structure are to be stored in a hash table
 * with linked lists as buckets, hence 'list' field here.
 */
struct klc_memblock_info
{
    struct hlist_node hlist;
    
    /* Pointer to the memory block and the size of that block.
     * 'size' is (size_t)(-1) if the block was freed rather than allocated
     */
    const void *block;
    size_t size;
    
    /* Call stack */
    unsigned int num_entries;
    unsigned long stack_entries[KEDR_MAX_FRAMES];
};

/* Helpers to create and destroy 'klc_memblock_info' structures. */

/* Creates and initializes klc_memblock_info structure and returns 
 * a pointer to it (or NULL if there is not enough memory). 
 * 
 * 'block_' is the pointer to the memory block, 'size_' is the size of that
 * block (should be -1 in case of free). These values will be stored 
 * in the corresponding fields of the structure.
 * A portion of call stack with depth no greater than 'max_stack_depth_'
 * will also be stored.
 * 'list' field will be initialized.
 *
 * This macro can be used in atomic context too (it uses GFP_ATOMIC flag 
 * when it allocates memory).
 */
#define klc_memblock_info_create(block_, size_, max_stack_depth_)   \
({                                                                  \
    struct klc_memblock_info *ptr;                                  \
    ptr = (struct klc_memblock_info *)kzalloc(                      \
        sizeof(struct klc_memblock_info),                           \
        GFP_ATOMIC);                                                \
    if (ptr != NULL) {                                              \
        ptr->block = (block_);                                      \
        ptr->size  = (size_);                                       \
        kedr_save_stack_trace(&(ptr->stack_entries[0]),             \
            (max_stack_depth_),                                     \
            &ptr->num_entries);                                     \
        INIT_HLIST_NODE(&ptr->hlist);                               \
    }                                                               \
    ptr;                                                            \
})

/* Helper macros to create and initialize klc_memblock_info structures 
 * for memory allocation and deallocation events.
 */
#define klc_alloc_info_create(block_, size_, max_stack_depth_) \
    klc_memblock_info_create((block_), (size_), (max_stack_depth_))
    
#define klc_dealloc_info_create(block_, max_stack_depth_) \
    klc_memblock_info_create((block_), (size_t)(-1), (max_stack_depth_))

/* Destroys klc_memblock_info structure pointed to by 'ptr'.
 * No-op if 'ptr' is NULL.
 * [NB] Before destroying the structure, make sure you have removed it
 * from the list if it was there.
 */
#define klc_memblock_info_destroy(ptr) kfree(ptr)

#endif /* MEMBLOCK_INFO_H_1734_INCLUDED */
