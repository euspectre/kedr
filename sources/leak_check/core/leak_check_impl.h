/* leak_check_impl.h - definitions visible only to the implementation
 * of the LeakCheck core. */

#ifndef LEAK_CHECK_IMPL_H_1548_INCLUDED
#define LEAK_CHECK_IMPL_H_1548_INCLUDED

#include <linux/list.h>
#include <linux/spinlock.h>
#include <kedr/util/stack_trace.h>

struct module;
struct kedr_lc_resource_info;
struct kedr_lc_output;

/* An instance of struct kedr_leak_check ("LeakCheck object") is created for
 * and contains the data concerning the analysis of the module.
 * A LeakCheck object is identified by 'target' pointer. The object is 
 * created when the target has just loaded and is reinitialized and reused 
 * if the target is unloaded and loaded again. The object is not destroyed
 * when the target is unloaded (we need the contents of the object after 
 * that) but rather when LeakCheck itself is unloaded. All resources
 * owned by this instance are handled this way too, e.g. debugfs files.
 * 
 * For each target module and hence, for each LeakCheck object, a directory
 * is created in debugfs: <debugfs>/<LeakCheck_main_dir>/<target_name>.
 * The files in this directory contain information about the target module:
 * totals, information concerning resource leaks, etc. */
 
/* kedr_lc_resource_info structures are stored in a hash table with 
 * KEDR_RI_TABLE_SIZE buckets. */
#define KEDR_RI_HASH_BITS   10
#define KEDR_RI_TABLE_SIZE  (1 << KEDR_RI_HASH_BITS)
 
struct kedr_leak_check
{
	/* LeakCheck objects are stored in a hash table for faster lookups 
	 * by 'target'. */
	struct hlist_node hlist;
	
	/* The target module. */
	struct module *target;
	
	/* The name of the target. It is stored here to enable lookup of the 
	 * LeakCheck objects after the target us unloaded and then loaded
	 * again. */
	char *name;
	
	/* The output subsystem for this LeakCheck object */
	struct kedr_lc_output *output;
	
	/* The storage of kedr_lc_resource_info structures corresponding
	 * to the memory allocation events.
	 * Order of elements: last in - first found. */
	struct hlist_head allocs[KEDR_RI_TABLE_SIZE];
	
	/* The storage of kedr_lc_resource_info structures corresponding 
	 * to the memory deallocation events for which no allocation event
	 * has been found ("unallocated frees").
	 * Order of elements: last in - first found. */
	struct hlist_head bad_frees[KEDR_RI_TABLE_SIZE];
	
	/* A single-threaded (ordered) workqueue where the requests to 
	 * handle allocations and deallocations are placed. It takes care of
	 * serialization of access to the storage of kedr_lc_resource_info 
	 * structures. The requests are guaranteed to be serviced strictly 
	 * one-by-one, in FIFO order. 
	 *
	 * LeakCheck API (kedr_lc_handle_*) constitutes the "top half" of 
	 * processing of the allocation/deallocation events. The workqueue 
	 * provides the bottom half. The notions of "top" and "bottom" 
	 * halves are similar to those used in interrupt handling.
	 *
	 * When the target has executed its cleanup function and is about to
	 * unload, the workqueue should be flushed and our on_target_unload()
	 * handler would therefore wait for all pending requests to be 
	 * processed. After that, the storage can be accessed without 
	 * locking as the workqueue is empty and no replacement function can
	 * execute for the target module to add new requests to the 
	 * workqueue. */
	struct workqueue_struct *wq;
	
	/* Statistics: total number of the detected resource allocations,
	 * possible leaks and unallocated frees. */
	u64 total_allocs;
	u64 total_leaks;
	u64 total_bad_frees;
};

/* This structure contains data about a resource:
 * the pointer to the resource ('addr') and a portion of the call stack for
 * the appropriate call to an allocation or deallocation function 
 * ('stack_entries' array containing 'num_entries' meaningful elements).
 * 
 * The instances of this structure are to be stored in a hash table with 
 * linked lists as buckets, hence 'hlist' field here. */
struct kedr_lc_resource_info
{
	struct hlist_node hlist;
	
	/* The address of a resource in memory and the size of that
	 *  resource. 'size' is (size_t)(-1) if the resource was freed 
	 * rather than allocated. */
	const void *addr;
	size_t size;
	
	/* Call stack */
	unsigned int num_entries;
	unsigned long stack_entries[KEDR_MAX_FRAMES];
};

#endif /* LEAK_CHECK_IMPL_H_1548_INCLUDED */
