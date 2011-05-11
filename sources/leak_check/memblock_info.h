/* memblock_info.h 
 * Definition of the structure containing information about allocated 
 * or freed memory block: the pointer to that block, stack trace, etc.
 * 
 * 11.05.2011, Eugene A. Shatokhin <spectre@ispras.ru>:
 * Converted klc_memblock_info_create() to a function and moved it out
 * to mbi_ops.c.
 * 
 * 07.04.2011, Andrey V. Tsyvarev  <tsyvarev@ispras.ru>:
 * Macro 'klc_memblock_info_create' indirectly accepts 'caller_address'
 * parameter instead of using __builting_return_address(0).
 * In the future the macro may be rewritten into function.
 */

#ifndef MEMBLOCK_INFO_H_1734_INCLUDED
#define MEMBLOCK_INFO_H_1734_INCLUDED

#include <linux/list.h>
#include <kedr/util/stack_trace.h>

/* This structure contains data about a block of memory:
 * the pointer to that block ('block') and a portion of the call stack for
 * the appropriate call to an allocation or deallocation function 
 * ('stack_entries' array containing 'num_entries' meaningful elements).
 * 
 * The instances of this structure are to be stored in a hash table
 * with linked lists as buckets, hence 'hlist' field here.
 */
struct klc_memblock_info
{
	struct hlist_node hlist;
	
	/* Pointer to the memory block and the size of that block.
	 * 'size' is (size_t)(-1) if the block was freed rather than 
	 * allocated. */
	const void *block;
	size_t size;
	
	/* Call stack */
	unsigned int num_entries;
	unsigned long stack_entries[KEDR_MAX_FRAMES];
};

#endif /* MEMBLOCK_INFO_H_1734_INCLUDED */
