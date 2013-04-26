/* leak_check.c - implementation of the LeakCheck core */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 *
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/hash.h>
#include <linux/workqueue.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <kedr/core/kedr.h>
#include <kedr/leak_check/leak_check.h>
#include <kedr/util/stack_trace.h>

#include "leak_check_impl.h"
#include "klc_output.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Default number of stack frames to output (at most) */
#define KEDR_STACK_DEPTH_DEFAULT KEDR_MAX_FRAMES

/* At most 'max_stack_entries' stack entries will be output for each 
 * suspicious allocation or deallocation. 
 * Should not exceed KEDR_MAX_FRAMES (see <kedr/util/stack_trace.h>). */
unsigned int stack_depth = KEDR_STACK_DEPTH_DEFAULT;
module_param(stack_depth, uint, S_IRUGO);

/* If non-zero, the results will be output not only to the files in 
 * debugfs but also to the system log. */
unsigned int syslog_output = 1;
module_param(syslog_output, uint, S_IRUGO);

/* The maximum number of the groups of bad free events that can be stored.
 * A group is two or more events with the same call stack. If more than
 * that number of bad free groups is detected, the newest ones are
 * discarded. 
 * 'total_bad_frees' will reflect the total number of bad free events 
 * nevertheless. */
unsigned int bad_free_groups_stored = 8;
module_param(bad_free_groups_stored, uint, S_IRUGO);
/* ====================================================================== */

/* LeakCheck objects are kept in a hash table for faster lookup, 'target'
 * is the key. 
 * It is expected that more than several modules will rarely be analyzed 
 * simultaneously even in the future. So a table with 32 buckets and
 * a decent hash function would be enough to make lookup reasonably fast. */
#define KEDR_LC_HASH_BITS 5
#define KEDR_LC_TABLE_SIZE  (1 << KEDR_LC_HASH_BITS)
static struct hlist_head lc_objects[KEDR_LC_TABLE_SIZE];

/* Access to 'lc_objects' should be done with 'lc_objects_lock' locked. 
 * 'lc_objects' can be accessed both from the public LeakCheck API 
 * (*handle_alloc, *handle_free) and from target_load/target_unload handlers
 * (adding new objects). The API can be called in atomic context too, so 
 * a mutex cannot be used here. 
 * 
 * [NB] This spinlock protects only the hash table to serialize lookup and
 * modification of the table. As soon as a pointer to the appropriate 
 * LeakCheck object is found, the object can be accessed without this 
 * spinlock held. */
static DEFINE_SPINLOCK(lc_objects_lock);
/* ====================================================================== */

/* Number of LeakCheck objects that are currently live. Used, for example,
 * to provide unique names for the workqueues used by these objects. 
 * Should be accessed only with 'lc_mutex' locked. */
static unsigned int obj_count = 0;

/* A mutex to serialize the execution of the target load handler here. KEDR
 * core may have serialized that already but it is not guaranteed and is 
 * subject to change. 
 * 
 * Specifically, 'lc_mutex' serializes the creation of LeakCheck objects.
 * Note that if a function also accesses the table of the objects, it should
 * lock 'lc_objects_lock' even if 'lc_mutex' is already taken.
 *
 * This mutex also guarantees that the results for a given target module are 
 * output "atomically" w.r.t. the results for other target modules. This is
 * especially useful for the output to the system log. Note that other 
 * kernel messages may still go in between the lines output to the system
 * log for the target. The mutex only guarantees that the results for 
 * different targets do not get intermingled. 
 *
 * In particular, klc_print_*() functions must be called with this mutex 
 * locked. */
static DEFINE_MUTEX(lc_mutex);

/* Maximum length of the name to be given to a workqueue created by 
 * LeakCheck. */
#define KLC_WQ_NAME_MAX_LEN 20

/* Format of workqueue names. */
static const char *wq_name_fmt = "kedr_lc%u";
/* ====================================================================== */

/* This structure represents a request to handle allocation or 
 * deallocation event or a request to flush the current results. */
struct klc_work {
	struct work_struct work;
	struct kedr_leak_check *lc;
	struct kedr_lc_resource_info *ri;
};

/* A spinlock that protects the top half of alloc/free handling. 
 * 
 * [NB] It appears that the implementation of save_stack_trace() is not 
 * guaranteed to be thread-safe as of this writing if the kernel uses DWARF2
 * unwinding. save_stack_trace() can be called when LeakCheck creates 
 * kedr_lc_resource_info instances. For the present, I cannot guarantee 
 * though, that serialization of the top halves of alloc/free handling 
 * makes things safe. This problem needs more investigation.
 * 
 * Note that the work queues take care of the bottom half, so it is not 
 * necessary to additionally protect that. Each workqueue is single-threaded
 * and its items access only the data owned by the same LeakCheck object 
 * that owns the work queue. So, the workqueues for different LeakCheck 
 * objects do not need additional synchronization. */
static DEFINE_SPINLOCK(top_half_lock);
/* ====================================================================== */

/* Creates and initializes kedr_lc_resource_info structure and returns
 * a pointer to it (or NULL if there is not enough memory).
 *
 * 'addr' is the pointer to the resource that has been allocated or freed,
 * 'size' is the size of that resource (should be -1 in case of free).
 * 'caller_address' is the return address of the call that performs
 * allocation or deallocation of the resource.
 *
 * This function can be used in atomic context too. */
static struct kedr_lc_resource_info *
resource_info_create(const void *addr, size_t size,
	const void *caller_address)
{
	struct kedr_lc_resource_info *info;
	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	if (info != NULL) {
		info->addr = addr;
		info->size  = size;
		kedr_save_stack_trace(&(info->stack_entries[0]),
			stack_depth,
			&info->num_entries,
			(unsigned long)caller_address);
		INIT_HLIST_NODE(&info->hlist);
	}
	return info;
}

/* Destroys kedr_lc_resource_info instance pointed to by 'info'.
 * No-op if 'info' is NULL.
 * [NB] Before destroying the structure, make sure it is not on the work
 * queue and you have removed it from the table if it was there. */
static void
resource_info_destroy(struct kedr_lc_resource_info *info)
{
	kfree(info);
}
/* ====================================================================== */

/* klc_clear_* can be called only in the following contexts to avoid messing
 * up the alloc/dealloc tables:
 * - when noone can post work items to lc->wq or
 * - from a work function of lc->wq (because the wq is single-threaded and
 *   ordered). */
static void
klc_clear_allocs(struct kedr_leak_check *lc)
{
	struct kedr_lc_resource_info *ri = NULL;
	struct hlist_head *head = NULL;
	unsigned int i;

	for (i = 0; i < KEDR_RI_TABLE_SIZE; ++i) {
		head = &lc->allocs[i];
		while (!hlist_empty(head)) {
			ri = hlist_entry(head->first,
				struct kedr_lc_resource_info, hlist);
			hlist_del(&ri->hlist);
			resource_info_destroy(ri);
		}
	}
}

static void
klc_clear_deallocs(struct kedr_leak_check *lc)
{
	struct kedr_lc_resource_info *ri = NULL;
	unsigned int i;

	if (lc->total_bad_frees == 0) {
		BUG_ON(lc->nr_bad_free_groups != 0);
		return;
	}
	BUG_ON(lc->nr_bad_free_groups == 0);

	/* No need to protect the storage because this function is called
	 * when no other code (replacement functions, etc.) can interfere.
	 */
	for (i = 0; i < lc->nr_bad_free_groups; ++i) {
		resource_info_destroy(ri);
		lc->bad_free_groups[i].ri = NULL;
	}
	lc->nr_bad_free_groups = 0;
}
/* ====================================================================== */

/* Creates a LeakCheck object for the specified target module. NULL is 
 * returned in case of failure. 
 * Should be called with 'lc_mutex' locked but not 'lc_objects_lock'
 * because the function may sleep.
 * [NB] This function does not access the hash table of LeakCheck objects.*/
static struct kedr_leak_check *
lc_object_create(struct module *target)
{
	struct kedr_leak_check *lc;
	unsigned int i;
	char wq_name[KLC_WQ_NAME_MAX_LEN + 1];
	
	BUG_ON(target == NULL);
	
	lc = kzalloc(sizeof(*lc), GFP_KERNEL);
	if (lc == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"Failed to create LeakCheck object: not enough memory\n");
		return NULL;
	}
		
	INIT_HLIST_NODE(&lc->hlist);
	lc->target = target;
	
	lc->name = kstrdup(module_name(target), GFP_KERNEL);
	if (lc->name == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"Not enough memory to create a copy of the target name.\n");
		goto out;
	}
	
	lc->output = kedr_lc_output_create(target, lc);
	BUG_ON(lc->output == NULL);
	if (IS_ERR(lc->output)) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"Failed to create output object, error code: %d\n",
			(int)PTR_ERR(lc->output));
		goto out_free_name;
	}
	
	for (i = 0; i < KEDR_RI_TABLE_SIZE; ++i)
		INIT_HLIST_HEAD(&lc->allocs[i]);

	lc->bad_free_groups = kzalloc(bad_free_groups_stored * 
		sizeof(struct kedr_lc_bad_free_group), GFP_KERNEL);
	if (lc->bad_free_groups == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"Not enough memory to create 'bad_free_groups'.\n");
		goto out_clean_output;
	}
	/* nr_bad_free_groups is now 0. */
	
	snprintf(&wq_name[0], KLC_WQ_NAME_MAX_LEN + 1, wq_name_fmt, 
		obj_count);
	++obj_count;
	lc->wq = create_singlethread_workqueue(wq_name);
	if (lc->wq == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"Failed to create the workqueue \"%s\"\n",
			wq_name);
		goto out_free_bad_frees;
	}
	
	kedr_lc_print_target_info(lc->output, target);

	/* [NB] The totals are already zero due to kzalloc(). */
	return lc;

out_free_bad_frees:
	kfree(lc->bad_free_groups);
out_clean_output:
	kedr_lc_output_destroy(lc->output);
out_free_name:
	kfree(lc->name);
out:
	kfree(lc);
	return NULL;
}

/* Destroys the LeakCheck object and releases the memory it occupies. 
 * [NB] Before calling this function, remove the object from the table if 
 * it was there. */
static void
lc_object_destroy(struct kedr_leak_check *lc)
{
	unsigned int i;
	
	if (lc == NULL)
		return;
	
	if (lc->wq != NULL) {
		flush_workqueue(lc->wq); /* just in case  */
		destroy_workqueue(lc->wq);
	}

	klc_clear_allocs(lc);
	klc_clear_deallocs(lc);
	
	/* The table of resource leaks should be already empty.
	 * Warn if it is not. */
	for (i = 0; i < KEDR_RI_TABLE_SIZE; ++i)
		WARN_ON_ONCE(!hlist_empty(&lc->allocs[i]));
	
	kfree(lc->bad_free_groups);
	kedr_lc_output_destroy(lc->output);
	kfree(lc->name);
	kfree(lc);
}

/* Reinitializes the specified LeakCheck object: clears the data accumulated
 * from the previous analysis session for the same target module, resets the
 * totals, etc. This is necessary when the target module is unloaded and 
 * then loaded again. */
static void
lc_object_reset(struct kedr_leak_check *lc)
{
	kedr_lc_output_clear(lc->output);
	if (lc->target != NULL)
		kedr_lc_print_target_info(lc->output, lc->target);

	klc_clear_allocs(lc);
	klc_clear_deallocs(lc);
	
	lc->nr_bad_free_groups = 0;
	lc->total_allocs = 0;
	lc->total_leaks = 0;
	lc->total_bad_frees = 0;
}

/* Looks for a LeakCheck object for the specified target in the table. 
 * Returns the pointer to the object if found, NULL otherwise. 
 * May be called from atomic context. 
 *
 * This function may be used in alloc/free handlers and other functions
 * except the target load handler (the old target address is invalid there).
 * The target load handler should use lc_object_for_target() instead. */
static struct kedr_leak_check *
lc_object_lookup(struct module *target)
{
	unsigned long irq_flags;
	struct kedr_leak_check *lc = NULL;
	struct kedr_leak_check *obj = NULL;
	struct hlist_head *head = NULL;
	struct hlist_node *node = NULL;
	
	if (target == NULL)
		return NULL;
	
	spin_lock_irqsave(&lc_objects_lock, irq_flags);
	head = &lc_objects[hash_ptr(target, KEDR_LC_HASH_BITS)];
	hlist_for_each_entry(obj, node, head, hlist) {
		if (obj->target == target) {
			lc = obj;
			break;
		}
	}
	spin_unlock_irqrestore(&lc_objects_lock, irq_flags);
	return lc;
}

/* Checks if the table contains an object for the target module but unlike 
 * lc_object_lookup(), target name and the names stored in the objects are
 * compared. If found, the function updates the address of struct module 
 * for the target in the object, moves the object to the appropriate bucket 
 * of the table according to that address. 
 * Returns the pointer to the object if found, NULL otherwise.
 *
 * This function could be slower than lc_object_lookup(). But as the number 
 * of target modules is expected to be not very large (usually, no more than
 * several dozens), it should not be much of a problem.
 * 
 * [NB] If comparing strings is actually a problem, something like strings 
 * with hashes could be used here to speed things up (see full_name_hash()
 * in <linux/dcache.h> for details).
 * 
 * This function should be used in the target load handler only. Other 
 * functions should use lc_object_lookup(). */
static struct kedr_leak_check *
lc_object_for_target(struct module *target)
{
	unsigned long irq_flags;
	struct kedr_leak_check *lc = NULL;
	struct kedr_leak_check *obj = NULL;
	struct hlist_head *head = NULL;
	struct hlist_node *node = NULL;
	struct hlist_node *tmp = NULL;
	unsigned int i;
	unsigned int new_bucket;
	const char *name = module_name(target);
	
	if (target == NULL)
		return NULL;
	
	new_bucket = hash_ptr(target, KEDR_LC_HASH_BITS);
	
	spin_lock_irqsave(&lc_objects_lock, irq_flags);
	for (i = 0; i < KEDR_LC_TABLE_SIZE; ++i) {
		head = &lc_objects[i];
		hlist_for_each_entry_safe(obj, node, tmp, head, hlist) {
			if (strcmp(name, obj->name) != 0)
				continue;
			
			lc = obj;
			lc->target = target;
			if (i != new_bucket) {
				hlist_del(&lc->hlist);
				hlist_add_head(&lc->hlist, 
					&lc_objects[new_bucket]);
			}
			break;
		}
	}
	spin_unlock_irqrestore(&lc_objects_lock, irq_flags);
	return lc;
}

/* Destroys all LeakCheck objects created so far and empties the table. This   
 * function should be called when LeakCheck itself is about to unload, so
 * it is no longer necessary to use 'lc_objects_lock'. */
static void
delete_all_lc_objects(void)
{
	struct hlist_head *head;
	struct kedr_leak_check *lc;
	struct hlist_node *node = NULL;
	struct hlist_node *tmp = NULL;
	unsigned int i;
	
	for (i = 0; i < KEDR_LC_TABLE_SIZE; ++i) {
		head = &lc_objects[i];
		hlist_for_each_entry_safe(lc, node, tmp, head, hlist) {
			hlist_del(&lc->hlist);
			lc_object_destroy(lc);
		}
	}
}
/* ====================================================================== */

/* Non-zero for the addresses that may belong to the user space, 
 * 0 otherwise. If the address is valid and this function returns non-zero,
 * it is an address in the user space. */
static int
is_user_space_address(unsigned long addr)
{
	return (addr < TASK_SIZE);
}

/* Returns 0 if the call stacks in the given kedr_lc_resource_info 
 * structures are not equal, non-zero otherwise. */
static int
call_stacks_equal(const struct kedr_lc_resource_info *lhs, 
	const struct kedr_lc_resource_info *rhs)
{
	unsigned int i;
	if (lhs->num_entries != rhs->num_entries)
		return 0;
	
	for (i = 0; i < lhs->num_entries; ++i) {
		/* The "outermost" call stack elements for a system call
		 * may be different for different processes. If the call
		 * stacks differ in such elements only, we still consider
		 * them equal. */
		if (is_user_space_address(lhs->stack_entries[i]) &&
		    is_user_space_address(rhs->stack_entries[i]))
			break;
		
		if (lhs->stack_entries[i] != rhs->stack_entries[i])
			return 0;
	}
	return 1;
}

static void 
ri_add(struct kedr_lc_resource_info *ri, struct hlist_head *ri_table)
{
	struct hlist_head *head;
	head = &ri_table[hash_ptr((void *)(ri->addr), KEDR_RI_HASH_BITS)];
	hlist_add_head(&ri->hlist, head);
}

static void
ri_add_bad_free(struct kedr_lc_resource_info *ri, 
	struct kedr_leak_check *lc)
{
	unsigned int i = 0;
	for (i = 0; i < lc->nr_bad_free_groups; ++i) {
		if (call_stacks_equal(ri, lc->bad_free_groups[i].ri)) {
			/* Similar events have already been stored. */
			++lc->bad_free_groups[i].nr_items;
			resource_info_destroy(ri);
			return;
		}
	}
	
	if (lc->nr_bad_free_groups == bad_free_groups_stored) {
		/* No space for a new group. */
		resource_info_destroy(ri);
		return;
	}
	
	++lc->nr_bad_free_groups;
	
	lc->bad_free_groups[i].ri = ri;
	lc->bad_free_groups[i].nr_items = 1;
}

/* A helper function that looks for an item with 'addr' field equal
 * to 'addr' in the table. If found, the item is removed from the table and
 * a pointer to it is returned. If not found, NULL is returned. */
static struct kedr_lc_resource_info *
ri_find_and_remove(const void *addr, struct hlist_head *ri_table)
{
	struct hlist_head *head;
	struct kedr_lc_resource_info *ri = NULL;
	struct kedr_lc_resource_info *found = NULL;
	struct hlist_node *node = NULL;
	struct hlist_node *tmp = NULL;
	
	head = &ri_table[hash_ptr((void *)addr, KEDR_RI_HASH_BITS)];
	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->addr == addr) {
			hlist_del(&ri->hlist);
			found = ri;
			break;
		}
	}
	return found;
}

/* Finds the items in the given table (starting from the bucket #start_index
 * and going until its end) that have the same call stack as 'ri' and marks
 * them as such (sets their 'num_similar' fields to -1). Returns the number
 * of such items in 'ri->num_similar'. */
static void
ri_count_similar(struct kedr_lc_resource_info *ri,
	struct hlist_head *ri_table, unsigned int start_index)
{
	unsigned int i;
	struct kedr_lc_resource_info *info = NULL;
	struct hlist_node *node = NULL;
	struct hlist_node *tmp = NULL;

	ri->num_similar = 0;
	
	for (i = start_index; i < KEDR_RI_TABLE_SIZE; ++i) {
		hlist_for_each_entry_safe(info, node, tmp, &ri_table[i], 
			hlist) {
			if (ri != info && call_stacks_equal(ri, info)) {
				++ri->num_similar;
				info->num_similar = (unsigned int)(-1);
			}
		}
	}
}

/* This function is usually called from deallocation handlers.
 * It looks for the item in the storage corresponding to the allocation
 * event with 'addr' field equal to 'addr'.
 * If it is found, i.e. if a matching allocation event is found, 
 * the function removes the item from the storage, deletes the item itself 
 * (no need to store it any longer) and returns nonzero.
 * Otherwise, the function returns 0 and leaves the storage unchanged.
 *
 * 'addr' must not be NULL. */
static int 
find_and_remove_alloc(const void *addr, struct kedr_leak_check *lc)
{
	int ret = 0;
	struct kedr_lc_resource_info *ri = NULL;
	
	WARN_ON(addr == NULL);
	
	ri = ri_find_and_remove(addr, &lc->allocs[0]);
	if (ri) {
		ret = 1;
		resource_info_destroy(ri);
		--lc->total_leaks;
	}
	return ret;
}
/* ====================================================================== */

/* If klc_flush_* functions are called from the "target unload" handler 
 * or from a work item in lc->wq, no locks are needed to protect
 * the accesses to the tables of allocation and deallocation information.
 * This is because the wq is ordered and it is flushed in the "target
 * unload" handler before the functions are called. */

static void
klc_flush_allocs(struct kedr_leak_check *lc)
{
	struct kedr_lc_resource_info *ri = NULL;
	struct hlist_node *node = NULL;
	struct hlist_node *tmp = NULL;
	struct hlist_head *head = NULL;
	unsigned int i;
	
	if (syslog_output != 0 && lc->total_leaks != 0)
		pr_warning(KEDR_LC_MSG_PREFIX 
			"LeakCheck has detected possible memory leaks: \n");

	for (i = 0; i < KEDR_RI_TABLE_SIZE; ++i) {
		head = &lc->allocs[i];
		hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
			 ri->num_similar = 0;
		}
	} 
		
	for (i = 0; i < KEDR_RI_TABLE_SIZE; ++i) {
		head = &lc->allocs[i];
		hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
			if (ri->num_similar == (unsigned int)(-1))
				/* This entry is similar to some entry
				 * processed before. Nothing more to do. */
				continue;

	/* We output only the most recent allocation with a given call stack
	 * to reduce the needed size of the output buffer and to make the
	 * report more readable. */
			ri_count_similar(ri, &lc->allocs[0], i);
			kedr_lc_print_alloc_info(lc->output, ri,
						 (u64)ri->num_similar);
		}
	} 
}

static void
klc_flush_deallocs(struct kedr_leak_check *lc)
{
	struct kedr_lc_resource_info *ri = NULL;
	unsigned int i;
	u64 stored = 0;
	
	if (lc->total_bad_frees == 0) {
		return;
	}
		
	if (syslog_output != 0) {
		pr_warning(KEDR_LC_MSG_PREFIX 
"LeakCheck has detected deallocations without matching allocations.\n");
	}
	
	for (i = 0; i < lc->nr_bad_free_groups; ++i) {
		u64 similar = (u64)(lc->bad_free_groups[i].nr_items - 1);
		ri = lc->bad_free_groups[i].ri;
		stored += lc->bad_free_groups[i].nr_items;
		
		kedr_lc_print_dealloc_info(lc->output, ri, similar);
	}
	
	kedr_lc_print_dealloc_note(lc->output, stored, lc->total_bad_frees);	
}

static void
klc_flush_stats(struct kedr_leak_check *lc)
{
	kedr_lc_print_totals(lc->output, lc->total_allocs, lc->total_leaks,
		lc->total_bad_frees);
	/* If needed, the counters will be reset by lc_object_reset(). */
	
	if (syslog_output != 0)
		pr_warning(KEDR_LC_MSG_PREFIX
			"======== end of LeakCheck report ========\n");
}

static void
work_func_flush(struct work_struct *work)
{
	struct klc_work *klc_work =
	    container_of(work, struct klc_work, work);
	struct kedr_leak_check *lc = klc_work->lc;

	kedr_lc_output_clear(lc->output);
	if (lc->target != NULL)
		kedr_lc_print_target_info(lc->output, lc->target);

	klc_flush_allocs(lc);
	klc_flush_deallocs(lc);
	klc_flush_stats(lc);

	kfree(klc_work);
}

/* [NB] May be called in atomic context. */
static void
klc_do_flush(struct kedr_leak_check *lc)
{
	struct klc_work *klc_work;

	klc_work = kzalloc(sizeof(*klc_work), GFP_ATOMIC);
	if (klc_work == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX "klc_do_flush: "
	"not enough memory to create 'struct klc_work'\n");
		return;
	}

	klc_work->lc = lc;
	INIT_WORK(&klc_work->work, work_func_flush);
	queue_work(lc->wq, &klc_work->work);

}

void
kedr_lc_flush_results(struct kedr_leak_check *lc)
{
	if (mutex_lock_killable(&lc_mutex) != 0) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"kedr_lc_flush_results(): failed to lock mutex\n");
		return;
	}

	klc_do_flush(lc);

	/* Make sure all pending requests have been processed before
	 * going on. */
	flush_workqueue(lc->wq);
	mutex_unlock(&lc_mutex);
}
/* ====================================================================== */

static void
work_func_clear(struct work_struct *work)
{
	struct klc_work *klc_work =
	    container_of(work, struct klc_work, work);
	struct kedr_leak_check *lc = klc_work->lc;

	lc_object_reset(lc);
	kfree(klc_work);
}

/* [NB] May be called in atomic context. */
static void
klc_do_clear(struct kedr_leak_check *lc)
{
	struct klc_work *klc_work;

	klc_work = kzalloc(sizeof(*klc_work), GFP_ATOMIC);
	if (klc_work == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX "klc_do_clear: "
	"not enough memory to create 'struct klc_work'\n");
		return;
	}

	klc_work->lc = lc;
	INIT_WORK(&klc_work->work, work_func_clear);
	queue_work(lc->wq, &klc_work->work);

}

void
kedr_lc_clear(struct kedr_leak_check *lc)
{
	if (mutex_lock_killable(&lc_mutex) != 0) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"kedr_lc_clear(): failed to lock mutex\n");
		return;
	}

	klc_do_clear(lc);

	/* Make sure all pending requests have been processed before
	 * going on. */
	flush_workqueue(lc->wq);
	mutex_unlock(&lc_mutex);
}
/* ====================================================================== */

/* The table of the objects can be changed only here. 'lc_mutex' ensures
 * that each time on_target_load() is called, it sees the table in a 
 * consistent state. 
 * 'lc_objects_lock' is used to protect the table. For example, some 
 * replacement function might be looking for an object for another target 
 * module in the table right now. */
static void
on_target_load(struct module *m)
{
	struct kedr_leak_check *lc = NULL;
	unsigned long irq_flags;
	struct hlist_head *head = NULL;
	
	BUG_ON(m == NULL);
	
	if (mutex_lock_killable(&lc_mutex) != 0) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"on_target_load(): failed to lock mutex\n");
		return;
	}
	
	lc = lc_object_for_target(m);
	if (lc != NULL) { 
		/* There has been an analysis session for this target
		 * already, reset and reuse the corresponding object. */
		lc_object_reset(lc);
		goto out;
	}
	
	/* Create a new LeakCheck object and add it to the table. */
	lc = lc_object_create(m);
	if (lc == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX "on_target_load(): "
		"failed to create LeakCheck object for \"%s\"\n",
			module_name(m));
		goto out;
	}
	
	spin_lock_irqsave(&lc_objects_lock, irq_flags);
	head = &lc_objects[hash_ptr(m, KEDR_LC_HASH_BITS)];
	hlist_add_head(&lc->hlist, head);
	spin_unlock_irqrestore(&lc_objects_lock, irq_flags);

out:	
	mutex_unlock(&lc_mutex);
	return;
}

static void
on_target_unload(struct module *m)
{
	struct kedr_leak_check *lc = lc_object_lookup(m);
	if (lc == NULL) {
		WARN_ON_ONCE(1);
		return;
	}

	if (mutex_lock_killable(&lc_mutex) != 0) {
		pr_warning(KEDR_LC_MSG_PREFIX
		"on_target_unload(): failed to lock mutex\n");
		return;
	}

	klc_do_flush(lc);

	/* Make sure all pending requests have been processed before
	 * going on. */
	flush_workqueue(lc->wq);

	lc->target = NULL;
	mutex_unlock(&lc_mutex);
}

/* [NB] Both the LeakCheck core and the payloads need to be notified when
 * the target has loaded or is about to unload. 
 * The LeakCheck core itself is also a payload module for KEDR, so it will 
 * receive appropriate notifications. */
static struct kedr_payload payload = {
	.mod                    = THIS_MODULE,
	.pre_pairs              = NULL,
	.post_pairs             = NULL,
	.target_load_callback   = on_target_load,
	.target_unload_callback = on_target_unload
};
/* ====================================================================== */

/* In work_func_*() functions, we do not need to use locks to protect the
 * storage of 'kedr_lc_resource_info' structures and the counters because
 * the work queue is ordered and does all this serialization already. */
static void 
work_func_alloc(struct work_struct *work)
{
	struct klc_work *klc_work = 
		container_of(work, struct klc_work, work);
	struct kedr_lc_resource_info *info = klc_work->ri;
	struct kedr_leak_check *lc = klc_work->lc;
	
	BUG_ON(info == NULL);

	ri_add(info, &lc->allocs[0]);
	++lc->total_allocs;
	++lc->total_leaks;

	kfree(klc_work);
}

static void
work_func_free(struct work_struct *work)
{
	struct klc_work *klc_work = 
		container_of(work, struct klc_work, work);
	struct kedr_lc_resource_info *info = klc_work->ri;
	struct kedr_leak_check *lc = klc_work->lc;
	
	BUG_ON(info == NULL);
	
	if (!find_and_remove_alloc(info->addr, lc)) {
		ri_add_bad_free(info, lc);
		++lc->total_bad_frees;
	}
	else {
		resource_info_destroy(info);
	}
		
	kfree(klc_work);
}

/* The top half. Must be called with 'top_half_lock' held. */
static void 
klc_handle_event(struct kedr_leak_check *lc, const void *addr, size_t size, 
	const void *caller_address,
	void (*work_func)(struct work_struct *))
{
	struct klc_work *klc_work;
	struct kedr_lc_resource_info *ri;
	
	ri = resource_info_create(addr, size, caller_address);
	if (ri == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX "klc_handle_event: "
	"not enough memory to create 'struct kedr_lc_resource_info'\n");
		return;
	}
	
	klc_work = kzalloc(sizeof(*klc_work), GFP_ATOMIC);
	if (klc_work == NULL) {
		pr_warning(KEDR_LC_MSG_PREFIX "klc_handle_event: "
	"not enough memory to create 'struct klc_work'\n");
		resource_info_destroy(ri);
		return;
	}
	
	klc_work->lc = lc;
	klc_work->ri = ri;
	INIT_WORK(&klc_work->work, work_func);
	queue_work(lc->wq, &klc_work->work);
}
/* ====================================================================== */

void
kedr_lc_handle_alloc(struct module *mod, const void *addr, size_t size, 
	const void *caller_address)
{
	unsigned long irq_flags;
	struct kedr_leak_check *lc;
	
	lc = lc_object_lookup(mod);
	if (lc == NULL) {
		/* An error has probably occured in LeakCheck before. */
		WARN_ON_ONCE(1);
		return;
	}
	
	spin_lock_irqsave(&top_half_lock, irq_flags);
	klc_handle_event(lc, addr, size, caller_address, work_func_alloc);
	spin_unlock_irqrestore(&top_half_lock, irq_flags);
}
EXPORT_SYMBOL(kedr_lc_handle_alloc);

void
kedr_lc_handle_free(struct module *mod, const void *addr, 
	const void *caller_address)
{
	unsigned long irq_flags;
	struct kedr_leak_check *lc;
	
	lc = lc_object_lookup(mod);
	if (lc == NULL) {
		/* An error has probably occured in LeakCheck before. */
		WARN_ON_ONCE(1);
		return;
	}
	
	spin_lock_irqsave(&top_half_lock, irq_flags);
	klc_handle_event(lc, addr, (size_t)(-1), caller_address, 
		work_func_free);
	spin_unlock_irqrestore(&top_half_lock, irq_flags);
}
EXPORT_SYMBOL(kedr_lc_handle_free);
/* ====================================================================== */

static void __exit
leak_check_cleanup_module(void)
{
	/* Unregister from KEDR core first, then clean up the rest. */
	kedr_payload_unregister(&payload);
	delete_all_lc_objects();
	kedr_lc_output_fini();
}

static int __init
leak_check_init_module(void)
{
	int ret = 0;
	
	if (stack_depth == 0 || stack_depth > KEDR_MAX_FRAMES) {
		pr_err(KEDR_LC_MSG_PREFIX
		"Invalid value of 'stack_depth': %u (should be a positive "
		"integer not greater than %u)\n",
			stack_depth,
			KEDR_MAX_FRAMES
		);
		return -EINVAL;
	}
	
	if (bad_free_groups_stored == 0) {
		pr_err(KEDR_LC_MSG_PREFIX
	"Parameter 'bad_free_groups_stored' must have a non-zero value.\n"
		);
		return -EINVAL;
	}
	
	ret = kedr_lc_output_init();
	if (ret != 0)
		return ret;
	
	ret = kedr_payload_register(&payload);
	if (ret != 0) 
		goto fail_reg;
  
	return 0;

fail_reg:
	kedr_lc_output_fini();
	return ret;
}

module_init(leak_check_init_module);
module_exit(leak_check_cleanup_module);
