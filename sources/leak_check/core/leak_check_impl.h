/* leak_check_impl.h - definitions visible only to the implementation
 * of the LeakCheck core. */

#ifndef LEAK_CHECK_IMPL_H_1548_INCLUDED
#define LEAK_CHECK_IMPL_H_1548_INCLUDED

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
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

	/* Addresses of "init" and "core" areas of the target. */
	void *init;
	void *core;
	
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
	
	/* The storage of the information about the memory deallocation 
	 * events for which no allocation event has been found 
	 * ("unallocated frees", "bad frees").
	 * The actual number of the elements in this array is 
	 * 'nr_bad_free_groups'. */
	struct kedr_lc_bad_free_group *bad_free_groups;
	unsigned int nr_bad_free_groups;
	
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
 * The instances of this structure may be stored in a hash table with 
 * linked lists as buckets, hence 'hlist' field here. */
struct kedr_lc_resource_info
{
	struct hlist_node hlist;
	
	/* The address of a resource in memory and the size of that
	 *  resource. 'size' is (size_t)(-1) if the resource was freed 
	 * rather than allocated. */
	const void *addr;
	size_t size;

	/* Number of events with the similar call stack. */
	unsigned int num_similar;
	
	/* Call stack */
	unsigned int num_entries;
	unsigned long stack_entries[KEDR_MAX_FRAMES];
	
	/* Caller process info.
	 * Note that if an event happened in an interrupt handler, task_pid
	 * will be -1 and the contents of task_comm[] will be undefined. */
	char task_comm[TASK_COMM_LEN];
	pid_t task_pid;
};

/* This structure is used to store the information about the bad 
 * ("unallocated") frees in a LeakCheck object. The records with the same
 * call stack are combined into a single object of this type ("a group"). */
struct kedr_lc_bad_free_group
{
	/* The information about the event. */
	struct kedr_lc_resource_info *ri;
	
	/* Number of the bad free events with the same call stack in 
	 * this group. */
	unsigned long nr_items;
};

#define KEDR_LC_MSG_PREFIX "[leak_check] "
extern unsigned int syslog_output;

/* "Flush" the current results of memory leak detection to make them
 * available in the files in debugfs. Note that the memory that was
 * allocated but not freed before this function was called will be reported
 * as leaked even if the target would free it later.
 * 
 * The function cannot be called from atomic context. */
void
kedr_lc_flush_results(struct kedr_leak_check *lc);

/* Clear the data about memory allocations and deallocations collected so
 * far. This is done automatically when the target module is loaded but it
 * may be necessary to do this explicitly in some cases.
 * 
 * The function cannot be called from atomic context. */
void
kedr_lc_clear(struct kedr_leak_check *lc);

#endif /* LEAK_CHECK_IMPL_H_1548_INCLUDED */
