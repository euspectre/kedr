/* mbi_ops.c
 * Operations with the storage for klc_memblock_info structures:
 * addition to and removal from the storage, searching, etc.
 * 
 * The goal is for the analysis system to use the interface to this storage
 * that does not heavily depend on the underlying data structures.
 */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Copyright (C) 2010-2011, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 * ====================================================================== */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include "memblock_info.h"
#include "mbi_ops.h"
#include "klc_output.h"
/* ====================================================================== */

/* klc_memblock_info structures are stored in a hash table with 
 * MBI_TABLE_SIZE buckets. */
#define MBI_HASH_BITS   10
#define MBI_TABLE_SIZE  (1 << MBI_HASH_BITS)

/* The storage of klc_memblock_info structures corresponding to memory 
 * allocation events.
 * Order of elements: last in - first found. */
static struct hlist_head alloc_table[MBI_TABLE_SIZE];

/* The storage of klc_memblock_info structures corresponding to memory 
 * deallocation events (only "unallocated frees" are stored).
 * Order of elements: last in - first found. */
static struct hlist_head bad_free_table[MBI_TABLE_SIZE];

/* A single-threaded (ordered) workqueue where the requests to handle 
 * allocations and deallocations are placed. It takes care of serialization
 * of access to the storage of klc_memblock_info structures. The requests 
 * are guaranteed to be serviced strictly one-by-one, in FIFO order. 
 *
 * klc_handle_target_unload() called from on_target_unload() handler will
 * flush the workqueue first, thus waiting for all pending requests to be 
 * processed. After that, the storage can be accessed without locking
 * as the workqueue is empty and no replacement function can execute to 
 * add new requests to it. */
static struct workqueue_struct *klc_wq = NULL;

/* This structure represents a request to handle allocation or 
 * deallocation. */
struct klc_work {
	struct work_struct work;
	struct klc_memblock_info *mbi;
};

/* A spinlock that protects top half of alloc/free handling. Note that the
 * work queue takes care of the bottom half, so it is not necessary to 
 * additionally protect that. */
DEFINE_SPINLOCK(handler_spinlock);

/* Statistics: total number of memory allocations, possible leaks and
 * unallocated frees. */
u64 total_allocs = 0;
u64 total_leaks = 0;
u64 total_bad_frees = 0;

/* ====================================================================== */
/* Creates and initializes klc_memblock_info structure and returns 
 * a pointer to it (or NULL if there is not enough memory). 
 * 
 * 'block' is the pointer to the memory block, 'size' is the size of that
 * block (should be -1 in case of free). These values will be stored 
 * in the corresponding fields of the structure.
 * A portion of call stack with depth no greater than 'max_stack_depth'
 * will also be stored.
 *
 * This function can be used in atomic context too.
 *
 * klc_memblock_info_create() can be used only in the functions that 
 * provide 'caller_address' variable, for example, in the pre- and post- 
 * handlers in the payload modules. */
static struct klc_memblock_info *
klc_memblock_info_create(const void *block, size_t size, 
	unsigned int max_stack_depth, const void *caller_address)
{
	struct klc_memblock_info *ptr;
	ptr = (struct klc_memblock_info *)kzalloc(
		sizeof(struct klc_memblock_info), GFP_ATOMIC);
	if (ptr != NULL) {
		ptr->block = block;
		ptr->size  = size;
		kedr_save_stack_trace(&(ptr->stack_entries[0]),
			max_stack_depth,
			&ptr->num_entries,
			(unsigned long)caller_address);
		INIT_HLIST_NODE(&ptr->hlist);
	}
	return ptr;
}

/* Destroys klc_memblock_info structure pointed to by 'ptr'.
 * No-op if 'ptr' is NULL.
 * [NB] Before destroying the structure, make sure it is not on the work
 * queue and you have removed it from the storage if it was there. */
static void
klc_memblock_info_destroy(struct klc_memblock_info *ptr)
{
	kfree(ptr);
}
/* ====================================================================== */
	
void 
klc_init_mbi_storage(void)
{
	unsigned int i = 0;
	for (; i < MBI_TABLE_SIZE; ++i) {
		INIT_HLIST_HEAD(&alloc_table[i]);
		INIT_HLIST_HEAD(&bad_free_table[i]);
	}
	return;
}

static void 
mbi_table_add(struct klc_memblock_info *mbi, struct hlist_head *mbi_table)
{
	struct hlist_head *head;
	head = &mbi_table[hash_ptr((void *)(mbi->block), MBI_HASH_BITS)];
	hlist_add_head(&mbi->hlist, head);
	return;
}

/* A helper function that looks for an item with 'block' field equal
 * to 'block'. If found, the item is removed from the storage and a pointer
 * to it is returned. If not found, NULL is returned.
 */
static struct klc_memblock_info *
mbi_find_and_remove_alloc(const void *block)
{
	struct hlist_head *head;
	struct klc_memblock_info *mbi = NULL;
	struct klc_memblock_info *found = NULL;
	struct hlist_node *node = NULL;
	struct hlist_node *tmp = NULL;
	
	head = &alloc_table[hash_ptr((void *)block, MBI_HASH_BITS)];
	hlist_for_each_entry_safe(mbi, node, tmp, head, hlist) {
		if (mbi->block == block) {
			hlist_del(&mbi->hlist);
			found = mbi;
			break;
		}
	}
	return found;
}

/* This function is usually called from deallocation handlers.
 * It looks for the item in the storage corresponding to the allocation
 * event with 'block' field equal to 'block'.
 * If it is found, i.e. if a matching allocation event is found, 
 * the function removes the item from the storage, deletes the item itself 
 * (no need to store it any longer) and returns nonzero.
 * Otherwise, the function returns 0 and leaves the storage unchanged.
 *
 * 'block' must not be NULL. */
static int 
klc_find_and_remove_alloc(const void *block)
{
	int ret = 0;
	struct klc_memblock_info *mbi = NULL;
	
	WARN_ON(block == NULL);
	
	mbi = mbi_find_and_remove_alloc(block);
	if (mbi) {
		ret = 1;
		klc_memblock_info_destroy(mbi);
		--total_leaks;
	}
	return ret;
}
/* ====================================================================== */

/* Returns 0 if the call stacks in the given klc_memblock_info structures
 * are not equal, non-zero otherwise. */
static int
call_stacks_equal(struct klc_memblock_info *lhs, 
	struct klc_memblock_info *rhs)
{
	unsigned int i;
	if (lhs->num_entries != rhs->num_entries)
		return 0;
	
	for (i = 0; i < lhs->num_entries; ++i) {
		if (lhs->stack_entries[i] != rhs->stack_entries[i])
			return 0;
	}
	return 1;
}

/* Call it after flushing the workqueue to make sure all pending operations
 * have completed. */
static void
klc_flush_allocs(void)
{
	struct klc_memblock_info *mbi = NULL;
	struct klc_memblock_info *pos = NULL;
	struct hlist_node *node = NULL;
	struct hlist_node *tmp = NULL;
	struct hlist_head *head = NULL;
	unsigned int i = 0;
	unsigned int k;

	/* No need to protect the storage because this function is called
	 * from on_target_unload handler when no replacement function can
	 * interfere and the workqueue has been flushed. */
	for (; i < MBI_TABLE_SIZE; ++i) {
		head = &alloc_table[i];
		while (!hlist_empty(head)) {
	/* We output only the most recent allocation with a given call stack 
	 * to reduce the needed size of the output buffer and to make the
	 * report more readable. */
			u64 similar_allocs = 0;
			mbi = hlist_entry(head->first, 
				struct klc_memblock_info, hlist);
			hlist_del(&mbi->hlist);
			
			for (k = i; k < MBI_TABLE_SIZE; ++k) {
				hlist_for_each_entry_safe(pos, node, tmp, 
					&alloc_table[k], hlist) {
					if (call_stacks_equal(mbi, pos)) {
						hlist_del(&pos->hlist);
						klc_memblock_info_destroy(pos);
						++similar_allocs;
					}
				}
			}
			
			klc_print_alloc_info(mbi, similar_allocs);
			klc_memblock_info_destroy(mbi);
		} /* end while */
	} /* end for */
}

/* Call it after flushing the workqueue to make sure all pending operations
 * have completed. */
static void
klc_flush_deallocs(void)
{
	struct klc_memblock_info *mbi = NULL;
	struct klc_memblock_info *pos = NULL;
	struct hlist_node *node = NULL;
	struct hlist_node *tmp = NULL;
	struct hlist_head *head = NULL;
	unsigned int i = 0;
	unsigned int k;

	/* No need to protect the storage because this function is called
	 * from on_target_unload handler when no replacement function can
	 * interfere and the workqueue has been flushed.*/
	for (; i < MBI_TABLE_SIZE; ++i) {
		head = &bad_free_table[i];
		while (!hlist_empty(head)) {
	/* We output only one bad deallocation with a given call stack 
	 * to reduce the needed size of the output buffer and to make the
	 * report more readable. */
			u64 similar_deallocs = 0;
			mbi = hlist_entry(head->first, 
				struct klc_memblock_info, hlist);
			hlist_del(&mbi->hlist);
			
			for (k = i; k < MBI_TABLE_SIZE; ++k) {
				hlist_for_each_entry_safe(pos, node, tmp, 
					&bad_free_table[k], hlist) {
					if (call_stacks_equal(mbi, pos)) {
						hlist_del(&pos->hlist);
						klc_memblock_info_destroy(pos);
						++similar_deallocs;
					}
				}
			}
			
			klc_print_dealloc_info(mbi, similar_deallocs);
			klc_memblock_info_destroy(mbi);
		} /* end while */
	} /* end for */
}

/* Call it after flushing the workqueue to make sure all pending operations
 * have completed. */
static void
klc_flush_stats(void)
{
	klc_print_totals(total_allocs, total_leaks, total_bad_frees);

	/* No need to protect these counters here as this function is called
	 * from on_target_unload handler when no replacement function can
	 * interfere and the workqueue has been flushed. */
	total_allocs = 0;
	total_leaks = 0;
	total_bad_frees = 0;
}
/* ====================================================================== */

void
klc_handle_target_load(struct module *target_module)
{
	klc_output_clear();
	klc_print_target_module_info(target_module);
	
	BUG_ON(klc_wq != NULL); /* just in case */
	klc_wq = create_singlethread_workqueue("kedr_leak_check_wq");
	if (klc_wq == NULL) {
		printk(KERN_ERR "[kedr_leak_check] klc_handle_target_load: "
		"failed to create the work queue (not enough memory?)\n");
	}
}

void
klc_handle_target_unload(struct module *not_used)
{
	/* If it were possible for the replacement functions or some 
	 * other facility to access the work queue in parallel with
	 * this function, a spinlock would be necessary to protect
	 * 'klc_wq' everywhere. For now, it is not. */
	if (klc_wq != NULL) {
		flush_workqueue(klc_wq);
		destroy_workqueue(klc_wq);
		klc_wq = NULL;
	}
	/* All pending tasks should have finished by now. */
	klc_flush_allocs();
	klc_flush_deallocs();
	klc_flush_stats();
}
/* ====================================================================== */

/* In work_func_*() functions, we do not need to use locks to protect the
 * storage of 'klc_memblock_info' structures and the counters because the 
 * work queue is ordered and does all this serialization already. */
static void 
work_func_alloc(struct work_struct *work)
{
	struct klc_work *klc_work = 
		container_of(work, struct klc_work, work);
	struct klc_memblock_info *alloc_info = klc_work->mbi;
	
	BUG_ON(alloc_info == NULL);

	mbi_table_add(alloc_info, alloc_table);
	++total_allocs;
	++total_leaks;

	kfree(klc_work);
}

static void
work_func_free(struct work_struct *work)
{
	struct klc_work *klc_work = 
		container_of(work, struct klc_work, work);
	struct klc_memblock_info *dealloc_info = klc_work->mbi;
	
	BUG_ON(dealloc_info == NULL);
	
	if (!klc_find_and_remove_alloc(dealloc_info->block)) {
		mbi_table_add(dealloc_info, bad_free_table);
		++total_bad_frees;
	}
		
	kfree(klc_work);
}

/* Must be called with handler_spinlock held: it appears that the 
 * implementation of save_stack_trace() is not guaranteed to be thread-safe
 * (and therefore, so are kedr_save_stack_trace() and 
 * klc_memblock_info_create()). */
static void 
klc_handle_event(const void *block, size_t size, 
	unsigned int max_stack_depth,
	const void *caller_address,
	void (*work_func)(struct work_struct *))
{
	struct klc_work *klc_work;
	struct klc_memblock_info *mbi;
	
	/* Do nothing if the initialization failed. */
	if (klc_wq == NULL)
		return;
	
	mbi = klc_memblock_info_create(block, size, max_stack_depth, 
		caller_address);
	if (mbi == NULL) {
		printk(KERN_ERR "[kedr_leak_check] klc_handle_event: "
	"not enough memory to create 'struct klc_memblock_info'\n");
		return;
	}
	
	klc_work = (struct klc_work *)kzalloc(sizeof(struct klc_work),
		GFP_ATOMIC);
	if (klc_work == NULL) {
		printk(KERN_ERR "[kedr_leak_check] klc_handle_event: "
	"not enough memory to create 'struct klc_work'\n");
		klc_memblock_info_destroy(mbi);
		return;
	}
	
	klc_work->mbi = mbi;
	INIT_WORK(&klc_work->work, work_func);
	queue_work(klc_wq, &klc_work->work);
}

void
klc_handle_alloc(const void *block, size_t size, 
	unsigned int max_stack_depth,
	const void *caller_address)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&handler_spinlock, irq_flags);
	klc_handle_event(block, size, max_stack_depth, 
		caller_address, work_func_alloc);
	spin_unlock_irqrestore(&handler_spinlock, irq_flags);
}

void
klc_handle_free(const void *block, unsigned int max_stack_depth,
	const void *caller_address)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&handler_spinlock, irq_flags);
	klc_handle_event(block, (size_t)(-1), max_stack_depth, 
		caller_address, work_func_free);
	spin_unlock_irqrestore(&handler_spinlock, irq_flags);
}
/* ====================================================================== */
