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
#include <linux/hardirq.h>

#include <kedr/core/kedr.h>
#include <kedr/leak_check/leak_check.h>
#include <kedr/util/stack_trace.h>

#include "leak_check_impl.h"
#include "klc_output.h"

#include "config.h"
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
/* Global leak check object. */
static struct kedr_leak_check* lc_object;

/* Rb-tree of stack entries.
 * 
 * It contains all stack entries which (may) require symbolic resolving.
 * Whenever someone request new stack entry for some address, the entry
 * is also inserted into the tree(unless tree already has entry with
 * given address).
 * When a section of the module is going to unload from the memory,
 * all entries corresponded to addresses within this section are
 * resolved. */
struct rb_root stack_entry_tree = RB_ROOT;
/* ====================================================================== */

/* A mutex to serialize the execution of the target load handler here. KEDR
 * core may have serialized that already but it is not guaranteed and is 
 * subject to change. 
 *
 * In particular, klc_print_*() functions must be called with this mutex 
 * locked. */
static DEFINE_MUTEX(lc_mutex);

/* Workqueue name for leak check object. */
static const char *wq_name = "kedr_lc";
/* ====================================================================== */

/* Non-zero if we are in IRQ (harqirq or softirq) context, 0 otherwise.
 * Equivalent to (in_irq() || in_serving_softirq()).
 * Unlike in_interrupt(), the function returns 0 in process context with BH
 * disabled (with spin_lock_bh(), etc.)
 * NMIs are not taken into account as the kernel modules usually do not
 * meddle with them.
 *
 * [NB] Some kernels prior to 2.6.37 may not define in_serving_softirq() but
 * everything needed to define it should be available. */
static inline unsigned long
kedr_in_interrupt(void)
{
	return in_irq() || (softirq_count() & SOFTIRQ_OFFSET);
}
/* ====================================================================== */

/* This structure represents a request to handle allocation or 
 * deallocation event or a request to flush the current results. */
struct klc_work {
	struct work_struct work;
	struct kedr_leak_check *lc;
	struct kedr_lc_resource_info *ri;
};

/* A spinlock that protects stack entries and 'stack_entry_tree'
 * from concurrent access. */
static DEFINE_SPINLOCK(stack_entry_lock);
/* ====================================================================== */

/* Symbolic value of stack entry, when it is failed to be allocated. */
static char stack_entry_symbolic_undef[] = "?";

/* Stack entry, when it is failed to be allocated. */
static struct stack_entry stack_entry_undef =
{
	.addr = 0,

	/* .node value needn't to be initialized, as it is not accessed
	 * via stack entry object. */

	/* Nobody owner this reference, so object cannot be destroyed via unref. */
	.refs = 1, 

	.symbolic = stack_entry_symbolic_undef
};

/* Resolve stack entry.
 * Should be called with 'stack_entry_lock' locked. */
void
stack_entry_resolve(struct stack_entry* entry)
{
	int len;
	if(entry->symbolic) return;

	len = snprintf(NULL, 0, "%pS", (void*)entry->addr);

	entry->symbolic = kmalloc(len + 1, GFP_ATOMIC);
	if(entry->symbolic)
	{
		snprintf(entry->symbolic, len + 1, "%pS", (void*)entry->addr);
	}
	else
	{
		entry->symbolic = stack_entry_symbolic_undef;
	}
}

/* Decrement reference count on stack entry object.
 * If it becomes 0, destroy obiect.
 * Should be called with 'stack_entry_lock' locked. */
static void
stack_entry_unref(struct stack_entry* entry)
{
	if(--entry->refs) return;

	if(entry->symbolic != stack_entry_symbolic_undef)
		kfree(entry->symbolic);

	kfree(entry);
}

/* Increment reference of the given stack entry.
 * Should be called with 'stack_entry_lock' locked. */
static struct stack_entry*
stack_entry_ref(struct stack_entry* entry)
{
	entry->refs++;

	return entry;
}

/* Create new or return existed stack entry object for given address.
 * Note: Always return non-NULL. 
 * Should be called with 'stack_entry_lock' locked. */
static struct stack_entry*
stack_entry_create(unsigned long addr)
{
	struct stack_entry* entry;
	struct rb_node ** new = &stack_entry_tree.rb_node, *parent = NULL;

	/* Search existing stack entry.*/
	while(*new)
	{
		entry = container_of(*new, typeof(*entry), node);
		parent = *new;

		if(entry->addr < addr)
			new = &((*new)->rb_right);
		else if(entry->addr > addr)
			new = &((*new)->rb_left);
		else
			return stack_entry_ref(entry);
	}

	/* If not found, create new one. */
	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if(!entry)
	{
		stack_entry_undef.refs++;
		return &stack_entry_undef;
	}

	entry->addr = addr;
	entry->refs = 2; /* One 'ref' for the tree, other for the caller. */
	entry->symbolic = NULL;

	/* And insert it into tree. */
	rb_link_node(&entry->node, parent, new);
	rb_insert_color(&entry->node, &stack_entry_tree);

	return entry;
}

/* Clear map of stack entries.
 * Should be called with 'stack_entry_lock' locked. */
static void
stack_entries_clear(void)
{
	struct stack_entry* entry;
	struct rb_node *node;

	for(node =  stack_entry_tree.rb_node;
		node;
		node =  stack_entry_tree.rb_node)
	{
		entry = container_of(node, typeof(*entry), node);
		rb_erase(node, &stack_entry_tree);
		stack_entry_unref(entry);
	}
}

/* Resolve and clear stack entries in the map within given range.
 * Should be called with 'stack_entry_lock' locked. */
static void
stack_entries_resolve_and_clear(unsigned long start, unsigned long end)
{
	struct stack_entry* entry;
	struct rb_node *node = stack_entry_tree.rb_node, *next = NULL;

	/* Search first entry with 'addr' >= start.
	 * After the loop it will be stored in 'next'.*/
	while(node)
	{
		entry = container_of(node, typeof(*entry), node);

		if(entry->addr >= start)
			{ next = node; node = node->rb_left; }
		else
			node = node->rb_right;
	}

	/* Do the work. */
	for(node = next; node; node = next)
	{
		entry = container_of(node, typeof(*entry), node);
		if(entry->addr >= end) break;

		next = rb_next(node);

		/* Resolve entry only if it used elsewhere outside of tree. */
		if(entry->refs > 1)
		{
			stack_entry_resolve(entry);
		}

		rb_erase(node, &stack_entry_tree);
		stack_entry_unref(entry);
	}
}

void
kedr_lc_resolve_stack_entries(struct stack_entry** entries,
	unsigned int num_entries)
{
	unsigned long flags;
	int i;
	spin_lock_irqsave(&stack_entry_lock, flags);

	for(i = 0; i < (int) num_entries; i++)
		stack_entry_resolve(entries[i]);

	spin_unlock_irqrestore(&stack_entry_lock, flags);
}

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
	unsigned long flags;

	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	if (info != NULL) {
		int i;
		unsigned long stack_addrs[ARRAY_SIZE(info->stack_entries)];
		// TODO: It seems that 'current' is valid even in interrupts.
		if (!kedr_in_interrupt()) {
			struct task_struct *task = current;
			info->task_pid = task_pid_nr(task);

			/*
			 * It seems that .comm field for 'current' task may be read
			 * without locks. At least, dump_stack_print_info() do that.
			 * 
			 * In the worst case, content of resulted info->tack_comm
			 * will be undefined array of characters.
			 * 
			 * task_lock() cannot be used there, as any IRQ-unsafe lock
			 * under IRQ-safe lock, even protected with
			 * kedr_in_interrupt(). See also comment for task_lock()
			 * implementation in linux/sched.h.
			 */
			strncpy(info->task_comm, task->comm,
				sizeof(task->comm));
		} else {
			info->task_pid = -1;
		}

		info->addr = addr;
		info->size  = size;

/* [NB] It appears that the implementation of save_stack_trace() is not 
 * guaranteed to be thread-safe as of this writing if the kernel uses DWARF2
 * unwinding. So, serialize its usage by Leak Check(using `stack_entry_lock`).
 * This problem needs more investigation. */

		spin_lock_irqsave(&stack_entry_lock, flags);

		kedr_save_stack_trace(stack_addrs,
			stack_depth,
			&info->num_entries,
			(unsigned long)caller_address);

		for(i = 0; i < info->num_entries; i++)
		{
			info->stack_entries[i] = stack_entry_create(stack_addrs[i]);
		}
		spin_unlock_irqrestore(&stack_entry_lock, flags);

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
	int i;
	unsigned long flags;

	if(!info) return;

	spin_lock_irqsave(&stack_entry_lock, flags);
	for(i = 0; i < info->num_entries; i++)
	{
		stack_entry_unref(info->stack_entries[i]);
	}
	spin_unlock_irqrestore(&stack_entry_lock, flags);

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
		resource_info_destroy(lc->bad_free_groups[i].ri);
		lc->bad_free_groups[i].ri = NULL;
	}
	lc->nr_bad_free_groups = 0;
}
/* ====================================================================== */

/* Creates a LeakCheck object. NULL is returned in case of failure. */
static struct kedr_leak_check *
lc_object_create(void)
{
	struct kedr_leak_check *lc;
	int i;
	
	lc = kzalloc(sizeof(*lc), GFP_KERNEL);
	if (lc == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX
		"Failed to create LeakCheck object: not enough memory\n");
		return NULL;
	}
	
	lc->output = kedr_lc_output_create(lc);
	BUG_ON(lc->output == NULL);
	if (IS_ERR(lc->output)) {
		pr_warn(KEDR_LC_MSG_PREFIX
		"Failed to create output object, error code: %d\n",
			(int)PTR_ERR(lc->output));
		goto fail_output;
	}
	
	for (i = 0; i < KEDR_RI_TABLE_SIZE; ++i)
		INIT_HLIST_HEAD(&lc->allocs[i]);

	lc->bad_free_groups = kzalloc(bad_free_groups_stored * 
		sizeof(struct kedr_lc_bad_free_group), GFP_KERNEL);
	if (lc->bad_free_groups == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX
		"Not enough memory to create 'bad_free_groups'.\n");
		goto fail_bad_free_groups;
	}
	/* nr_bad_free_groups is now 0. */
	lc->wq = create_singlethread_workqueue(wq_name);
	if (lc->wq == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX
		"Failed to create the workqueue \"%s\"\n",
			wq_name);
		goto fail_wq;
	}
	
	/* [NB] The totals are already zero due to kzalloc(). */
	return lc;

fail_wq:
	kfree(lc->bad_free_groups);
fail_bad_free_groups:
	kedr_lc_output_destroy(lc->output);
fail_output:
	kfree(lc);
	return NULL;
}

/* Destroys the LeakCheck object and releases the memory it occupies. */
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
	kfree(lc);
}

/* Reinitializes the specified LeakCheck object: clears the data accumulated
 * from the previous analysis session for the same target module, resets the
 * totals, etc. */
static void
lc_object_reset(struct kedr_leak_check *lc)
{
	kedr_lc_output_clear(lc->output);

	klc_clear_allocs(lc);
	klc_clear_deallocs(lc);
	
	lc->nr_bad_free_groups = 0;
	lc->total_allocs = 0;
	lc->total_leaks = 0;
	lc->total_bad_frees = 0;
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
		if (is_user_space_address(lhs->stack_entries[i]->addr) &&
		    is_user_space_address(rhs->stack_entries[i]->addr))
			break;
		
		if (lhs->stack_entries[i]->addr != rhs->stack_entries[i]->addr)
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
	struct hlist_node *tmp = NULL;
	
	head = &ri_table[hash_ptr((void *)addr, KEDR_RI_HASH_BITS)];
	kedr_hlist_for_each_entry_safe(ri, tmp, head, hlist) {
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
	struct hlist_node *tmp = NULL;

	ri->num_similar = 0;
	
	for (i = start_index; i < KEDR_RI_TABLE_SIZE; ++i) {
		kedr_hlist_for_each_entry_safe(info, tmp, &ri_table[i], 
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
	struct hlist_node *tmp = NULL;
	struct hlist_head *head = NULL;
	unsigned int i;
	
	if (syslog_output != 0 && lc->total_leaks != 0)
		pr_warn(KEDR_LC_MSG_PREFIX 
			"LeakCheck has detected possible memory leaks: \n");

	for (i = 0; i < KEDR_RI_TABLE_SIZE; ++i) {
		head = &lc->allocs[i];
		kedr_hlist_for_each_entry_safe(ri, tmp, head, hlist) {
			 ri->num_similar = 0;
		}
	} 
		
	for (i = 0; i < KEDR_RI_TABLE_SIZE; ++i) {
		head = &lc->allocs[i];
		kedr_hlist_for_each_entry_safe(ri, tmp, head, hlist) {
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
		pr_warn(KEDR_LC_MSG_PREFIX 
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
		pr_warn(KEDR_LC_MSG_PREFIX
			"======== end of LeakCheck report ========\n");
}

static void
work_func_flush(struct work_struct *work)
{
	struct klc_work *klc_work =
	    container_of(work, struct klc_work, work);
	struct kedr_leak_check *lc = klc_work->lc;

	kedr_lc_output_clear(lc->output);

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
		pr_warn(KEDR_LC_MSG_PREFIX "klc_do_flush: "
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
		pr_warn(KEDR_LC_MSG_PREFIX
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
		pr_warn(KEDR_LC_MSG_PREFIX "klc_do_clear: "
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
		pr_warn(KEDR_LC_MSG_PREFIX
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

static void
on_session_start(void)
{
	if (mutex_lock_killable(&lc_mutex) != 0) {
		pr_warn(KEDR_LC_MSG_PREFIX
		"on_target_load(): failed to lock mutex\n");
		return;
	}
	
	lc_object_reset(lc_object);
	mutex_unlock(&lc_mutex);
	return;
}

static void
on_session_end(void)
{
	unsigned long flags;

	if (mutex_lock_killable(&lc_mutex) != 0) {
		pr_warn(KEDR_LC_MSG_PREFIX
		"on_target_unload(): failed to lock mutex\n");
		return;
	}

	klc_do_flush(lc_object);

	/* Make sure all pending requests have been processed before
	 * going on. */
	flush_workqueue(lc_object->wq);

	/* Clear stack entries tree also.
	 * New session may involve completely different modules and
	 * addresses. */
	spin_lock_irqsave(&stack_entry_lock, flags);
	stack_entries_clear();
	spin_unlock_irqrestore(&stack_entry_lock, flags);

	mutex_unlock(&lc_mutex);
}

/* Callback just for prints information about target module.
 * May be this info will be helpful in futher analyze. */
static void
on_target_loaded(struct module* target)
{
	if (mutex_lock_killable(&lc_mutex) != 0) {
		pr_warn(KEDR_LC_MSG_PREFIX
		"on_target_unload(): failed to lock mutex\n");
		return;
	}

	kedr_lc_print_target_info(lc_object->output, target,
		module_init_addr(target), module_core_addr(target));

	mutex_unlock(&lc_mutex);
}

/* [NB] LeakCheck core need to be notified about session start/end. 
 * The LeakCheck core itself is also a payload module for KEDR, so it will 
 * receive appropriate notifications. */
static struct kedr_payload payload = {
	.mod                    = THIS_MODULE,
	.pre_pairs              = NULL,
	.post_pairs             = NULL,
	.on_session_start       = on_session_start,
	.on_session_end         = on_session_end,
	.on_target_loaded       = on_target_loaded
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

/* The top half. */
static void 
klc_handle_event(struct kedr_leak_check *lc, const void *addr, size_t size, 
	const void *caller_address,
	void (*work_func)(struct work_struct *))
{
	struct klc_work *klc_work;
	struct kedr_lc_resource_info *ri;
	
	ri = resource_info_create(addr, size, caller_address);
	if (ri == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX "klc_handle_event: "
	"not enough memory to create 'struct kedr_lc_resource_info'\n");
		return;
	}
	
	klc_work = kzalloc(sizeof(*klc_work), GFP_ATOMIC);
	if (klc_work == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX "klc_handle_event: "
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
kedr_lc_handle_alloc(const void *addr, size_t size, 
	const void *caller_address)
{
	klc_handle_event(lc_object, addr, size, caller_address, work_func_alloc);
}
EXPORT_SYMBOL(kedr_lc_handle_alloc);

void
kedr_lc_handle_free(const void *addr,
	const void *caller_address)
{
	klc_handle_event(lc_object, addr, (size_t)(-1), caller_address,
		work_func_free);
}
EXPORT_SYMBOL(kedr_lc_handle_free);
/* ====================================================================== */
/* A callback function to catch loading and unloading of module.
 * Resolve stack entries in the module's section, whet it is going to
 * be unloaded. */

static int
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	unsigned long flags;
	struct module* mod = (struct module *)vmod;

	switch(mod_state)
	{
	case MODULE_STATE_LIVE:
		/* .init section of the module going to be unloaded. */
		if(module_init_addr(mod))
		{
			flush_workqueue(lc_object->wq);

			spin_lock_irqsave(&stack_entry_lock, flags);
			stack_entries_resolve_and_clear(
				(unsigned long)module_init_addr(mod),
				(unsigned long)module_init_addr(mod) + init_size(mod));
			spin_unlock_irqrestore(&stack_entry_lock, flags);
		}
	break;
	case MODULE_STATE_GOING:
		/* all sections of the module going to be unloaded. */
		flush_workqueue(lc_object->wq);

		spin_lock_irqsave(&stack_entry_lock, flags);

		if(module_init_addr(mod))
		{
			stack_entries_resolve_and_clear(
				(unsigned long)module_init_addr(mod),
				(unsigned long)module_init_addr(mod) + init_size(mod));
		}

		stack_entries_resolve_and_clear(
			(unsigned long)module_core_addr(mod),
			(unsigned long)module_core_addr(mod) + core_size(mod));

		spin_unlock_irqrestore(&stack_entry_lock, flags);
	break;
	}

	return 0;
}

/* A struct for watching for loading/unloading of modules.*/
static struct notifier_block detector_nb = {
	.notifier_call = detector_notifier_call,
	.next = NULL,

	/* Let KEDR core do it job first. So, if last target module will
	 * be unloaded, stack_entries tree become empty when we try to
	 * resolve entries for that module. */
	.priority = -2, 
};


static void __exit
leak_check_cleanup_module(void)
{
	/* Unregister from KEDR core first, then clean up the rest. */
	kedr_payload_unregister(&payload);
	unregister_module_notifier(&detector_nb);
	lc_object_destroy(lc_object);
	stack_entries_clear(); // Just for the case.
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
	
	lc_object = lc_object_create();
	if (!lc_object)
		goto fail_lc_object;

	ret = register_module_notifier(&detector_nb);
	if (ret)
		goto fail_notifier;

	ret = kedr_payload_register(&payload);
	if (ret)
		goto fail_payload;
  
	return 0;

fail_payload:
	unregister_module_notifier(&detector_nb);
fail_notifier:
	lc_object_destroy(lc_object);
fail_lc_object:
	kedr_lc_output_fini();
	return ret;
}

module_init(leak_check_init_module);
module_exit(leak_check_cleanup_module);
