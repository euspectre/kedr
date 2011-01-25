/* mbi_ops.c
 * Operations with the storage for klc_memblock_info structures:
 * addition to and removal from the storage, searching, etc.
 * 
 * The goal is for the analysis system to use the interface to this storage
 * that does not heavily depend on the underlying data structures.
 */

/* ========================================================================
 * Copyright (C) 2010, Institute for System Programming 
 *                     of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/hash.h>

#include "mbi_ops.h"
#include "klc_output.h"

/* klc_memblock_info structures are stored in a has table with 
 * MBI_TABLE_SIZE buckets.
 */
#define MBI_HASH_BITS   10
#define MBI_TABLE_SIZE  (1 << MBI_HASH_BITS)

/* The storage of klc_memblock_info structures corresponding to memory 
 * allocation events.
 * Order of elements: last in - first found. 
 */
static struct hlist_head alloc_table[MBI_TABLE_SIZE];

/* The storage of klc_memblock_info structures corresponding to memory 
 * deallocation events (only "unallocated frees" are stored).
 * Order of elements: last in - first found. 
 */
static struct hlist_head bad_free_table[MBI_TABLE_SIZE];

/* A spinlock to serialize access to the storage of klc_memblock_info
 * structures.
 */
DEFINE_SPINLOCK(spinlock_mbi_storage);

/* Statistics: total number of memory allocations, possible leaks and
 * unallocated frees.
 */
u64 total_allocs = 0;
u64 total_leaks = 0;
u64 total_bad_frees = 0;
/* ================================================================ */

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

/* Must be called under 'spinlock_mbi_storage'. */
static void 
mbi_table_add(struct klc_memblock_info *mbi, struct hlist_head *mbi_table)
{
    struct hlist_head *head;
    head = &mbi_table[hash_ptr((void *)(mbi->block), MBI_HASH_BITS)];
    hlist_add_head(&mbi->hlist, head);
    return;
}

void
klc_add_alloc_impl(struct klc_memblock_info *alloc_info)
{
    unsigned long irq_flags;
    BUG_ON(alloc_info == NULL);

    spin_lock_irqsave(&spinlock_mbi_storage, irq_flags);
    mbi_table_add(alloc_info, alloc_table);
    ++total_allocs;
    ++total_leaks;
    spin_unlock_irqrestore(&spinlock_mbi_storage, irq_flags);
    return;    
}

void
klc_add_bad_free_impl(struct klc_memblock_info *dealloc_info)
{
    unsigned long irq_flags;
    BUG_ON(dealloc_info == NULL);

    spin_lock_irqsave(&spinlock_mbi_storage, irq_flags);
    mbi_table_add(dealloc_info, bad_free_table);
    ++total_bad_frees;
    spin_unlock_irqrestore(&spinlock_mbi_storage, irq_flags);
    return;    
}

int
klc_find_and_remove_alloc(const void *block)
{
    unsigned long irq_flags;
    struct hlist_head *head;
    struct klc_memblock_info *mbi = NULL;
    struct hlist_node *node = NULL;
    struct hlist_node *tmp = NULL;
    int ret = 0;
    
    WARN_ON(block == NULL);
    
    spin_lock_irqsave(&spinlock_mbi_storage, irq_flags);
    head = &alloc_table[hash_ptr((void *)block, MBI_HASH_BITS)];
    hlist_for_each_entry_safe(mbi, node, tmp, head, hlist) {
        if (mbi->block == block) {
            ret = 1;
            hlist_del(&mbi->hlist);
            klc_memblock_info_destroy(mbi);
            --total_leaks;
        }
    }
    spin_unlock_irqrestore(&spinlock_mbi_storage, irq_flags);
    return ret;    
}

void
klc_flush_allocs(void)
{
    struct klc_memblock_info *mbi = NULL;
    struct hlist_node *node = NULL;
    struct hlist_node *tmp = NULL;
    unsigned int i = 0;

    /* No need to protect the storage because this function is called
     * from on_target_unload handler when no replacement function can
     * interfere.*/
    for (; i < MBI_TABLE_SIZE; ++i) {
        hlist_for_each_entry_safe(mbi, node, tmp, &alloc_table[i], hlist) {
            klc_print_alloc_info(mbi);
            hlist_del(&mbi->hlist);
            klc_memblock_info_destroy(mbi);
        }
    }
    return;
}

void
klc_flush_deallocs(void)
{
    struct klc_memblock_info *mbi = NULL;
    struct hlist_node *node = NULL;
    struct hlist_node *tmp = NULL;
    unsigned int i = 0;

    /* No need to protect the storage because this function is called
     * from on_target_unload handler when no replacement function can
     * interfere.*/
    for (; i < MBI_TABLE_SIZE; ++i) {
        hlist_for_each_entry_safe(mbi, node, tmp, &bad_free_table[i], hlist) {
            klc_print_dealloc_info(mbi);
            hlist_del(&mbi->hlist);
            klc_memblock_info_destroy(mbi);
        }
    }
    return;
}

void
klc_flush_stats(void)
{
    klc_print_totals(total_allocs, total_leaks, total_bad_frees);

    /* No need to protect these counters here as this function is called
     * from on_target_unload handler when no replacement function can
     * interfere.
     */
    total_allocs = 0;
    total_leaks = 0;
    total_bad_frees = 0;
    return;
}
/* ================================================================ */
