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

#include "mbi_ops.h"
#include "klc_output.h"

/* A spinlock to serialize access to the storage of klc_memblock_info
 * structures.
 */
DEFINE_SPINLOCK(spinlock_mbi_storage);

/* The list of klc_memblock_info structures corresponding to memory 
 * allocation events.
 * Order of elements: LIFO.
 */
LIST_HEAD(alloc_list);

/* The list of klc_memblock_info structures corresponding to memory 
 * deallocation events (only "unallocated frees" are stored).
 * Order of elements: LIFO.
 */
LIST_HEAD(bad_free_list);

/* Statistics: total number of memory allocations, possible leaks and
 * unallocated frees.
 */
u64 total_allocs = 0;
u64 total_leaks = 0;
u64 total_bad_frees = 0;
/* ================================================================ */

void
klc_add_alloc_impl(struct klc_memblock_info *alloc_info)
{
    unsigned long irq_flags;
    BUG_ON(alloc_info == NULL);

    spin_lock_irqsave(&spinlock_mbi_storage, irq_flags);
    list_add(&alloc_info->list, &alloc_list);
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
    list_add(&dealloc_info->list, &bad_free_list);
    ++total_bad_frees;
    spin_unlock_irqrestore(&spinlock_mbi_storage, irq_flags);
    return;    
}

int
klc_find_and_remove_alloc(const void *block)
{
    unsigned long irq_flags;
    int ret = 0;
    struct klc_memblock_info *mbi = NULL;
    struct klc_memblock_info *tmp = NULL;
    
    WARN_ON(block == NULL);
    
    spin_lock_irqsave(&spinlock_mbi_storage, irq_flags);
    list_for_each_entry_safe(mbi, tmp, &alloc_list, list) {
        if (mbi->block == block) {
            ret = 1;
            list_del(&mbi->list);
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
    struct klc_memblock_info *tmp = NULL;
    
    list_for_each_entry_safe(mbi, tmp, &alloc_list, list) {
        klc_print_alloc_info(mbi);
        list_del(&mbi->list);
        klc_memblock_info_destroy(mbi);
    }
    return;
}

void
klc_flush_deallocs(void)
{
    struct klc_memblock_info *mbi = NULL;
    struct klc_memblock_info *tmp = NULL;
    
    list_for_each_entry_safe(mbi, tmp, &bad_free_list, list) {
        klc_print_dealloc_info(mbi);
        list_del(&mbi->list);
        klc_memblock_info_destroy(mbi);
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
