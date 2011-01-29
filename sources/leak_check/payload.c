/*********************************************************************
 * Module: kedr_leak_check
 *
 * This payload module checks the target for memory leaks.
 * kedr_leak_check does not use trace events or the like, it performs
 * analysis itself and simply outputs the results to the files in 
 * debugfs - see the contents of 'kedr_leak_check' directory there.
 *
 * For each possible memory leak (and for each free-like call without 
 * matching allocation call) the system stores the address of the memory
 * block and a portion of the call stack for the allocation / deallocation
 * call.
 *
 * Module parameters:
 *  stack_depth - (unsigned integer, not greater than 16) - maximum number
 *  of stack frames to store and output. Default: 7.
 *
 * Notes:
 * kedr_leak_check can be more convenient to look for memory leaks than 
 * plain call monitoring with analysis of the trace. The trace may become
 * huge if the target module actively allocates and frees memory.
 *********************************************************************/
 
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/vmalloc.h>

#include <asm/uaccess.h>

#include <kedr/base/common.h>

#include "memblock_info.h"
#include "klc_output.h"
#include "mbi_ops.h"

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");

/*********************************************************************
 * Parameters of the module
 *********************************************************************/
/* Default number of stack frames to output (at most) */
#define KEDR_STACK_DEPTH_DEFAULT 7

/* At most 'max_stack_entries' stack entries will be output for each 
 * suspicious allocation or deallocation. 
 * Should not exceed KEDR_MAX_FRAMES.
 */
unsigned int stack_depth = KEDR_STACK_DEPTH_DEFAULT;
module_param(stack_depth, uint, S_IRUGO);

/*********************************************************************
 * The callbacks to be called after the target module has just been
 * loaded and, respectively, when it is about to unload.
 *********************************************************************/
static void
target_load_callback(struct module *target_module)
{
    BUG_ON(target_module == NULL);
    
    klc_output_clear();
    klc_print_target_module_info(target_module);
    return;
}

static void
target_unload_callback(struct module *target_module)
{
    BUG_ON(target_module == NULL);
    
    klc_flush_allocs();
    klc_flush_deallocs();
    klc_flush_stats();
    return;
}

/*********************************************************************
 * Replacement functions
 * 
 * [NB] Each deallocation should be processed in a replacement function
 * BEFORE calling the target function.
 * Each allocation should be processed AFTER the call to the target 
 * function.
 * This allows to avoid some problems on multiprocessor systems. As soon
 * as a memory block is freed, it may become the result of a new allocation
 * made by a thread on another processor. If a deallocation is processed 
 * after it has actually been done, a race condition could happen because 
 * another thread could break in during that gap.
 *********************************************************************/

/*********************************************************************
 * "kmalloc" group
 *********************************************************************/
static void *
repl___kmalloc(size_t size, gfp_t flags)
{
    void *ret_val;
    ret_val = __kmalloc(size, flags);
    
    if (!ZERO_OR_NULL_PTR(ret_val))
        klc_add_alloc(ret_val, size, stack_depth);

    return ret_val;
}

static void *
repl_krealloc(const void *p, size_t size, gfp_t flags)
{
    void *ret_val;
    
    if (size == 0 || !ZERO_OR_NULL_PTR(p)) {
        /* kfree */
        if (!ZERO_OR_NULL_PTR(p) && !klc_find_and_remove_alloc(p)) 
            klc_add_bad_free(p, stack_depth);
/* [NB] If size != 0 and p != NULL and later the allocation fails, we will
 * need to add a fake allocation event for 'p' to the storage because 'p'
 * is not actually freed by krealloc() in this case.
 */
    }
    
    ret_val = krealloc(p, size, flags);

    if (size != 0) {
        if (p == NULL) { 
            /* kmalloc */
            if (ret_val != NULL)
                klc_add_alloc(ret_val, size, stack_depth);
        } else {
            /* kfree + kmalloc if everything succeeds */
            klc_add_alloc(((ret_val != NULL) ? ret_val : p), size, stack_depth);
            /* If the allocation failed, we return information about 'p'
             * to the storage. A minor issue is that stack trace will 
             * now point to this call to krealloc rather than to the call 
             * when 'p' was allocated. Should not be much of a problem.
             */
        }
    }
    return ret_val;
}

static void*
repl___krealloc(const void *p, size_t size, gfp_t flags)
{
    void *ret_val;
    ret_val = __krealloc(p, size, flags);
    
    if (size == 0) /* do nothing */
        return ret_val;
    
    if (p == NULL) { 
        /* same as kmalloc */
        if (ret_val != NULL)
            klc_add_alloc(ret_val, size, stack_depth);
    } else {
        /* this part is more tricky as __krealloc may or may not call
         * kmalloc and, in addition, it does not free 'p'.
         */
        if (ret_val != NULL && ret_val != p) /* allocation has been done */
            klc_add_alloc(ret_val, size, stack_depth);
    }
    return ret_val;
}

static void
repl_kfree(void *p)
{
    /* This is done before actually calling kfree(), because once the 
     * memory block pointed to by 'p' is freed, some other CPU might 
     * call an allocation function, get this address as a result and
     * add it to the storage before klc_find_and_remove_alloc() is called
     * below, which would make a mess.
     */
    if (!ZERO_OR_NULL_PTR(p) && !klc_find_and_remove_alloc(p)) 
        klc_add_bad_free(p, stack_depth);
    
    kfree(p);
    return;
}

static void
repl_kzfree(void *p)
{
    if (!ZERO_OR_NULL_PTR(p) && !klc_find_and_remove_alloc(p)) 
        klc_add_bad_free(p, stack_depth);
    
    kzfree(p);
    return;
}

static void *
repl_kmem_cache_alloc(struct kmem_cache *mc, gfp_t flags) 
{
    void *ret_val;
    ret_val = kmem_cache_alloc(mc, flags);

    if (ret_val != NULL)
        klc_add_alloc(ret_val, (size_t)kmem_cache_size(mc), stack_depth);
    
    return ret_val;
}

#ifdef KEDR_HAVE_KMCA_NOTRACE 
static void *
repl_kmem_cache_alloc_notrace(struct kmem_cache *mc, gfp_t flags) 
{
    void *ret_val;
    ret_val = kmem_cache_alloc_notrace(mc, flags);

    if (ret_val != NULL)
        klc_add_alloc(ret_val, (size_t)kmem_cache_size(mc), stack_depth);
    
    return ret_val;
}
#endif

static void
repl_kmem_cache_free(struct kmem_cache *mc, void *p)
{
    if (!ZERO_OR_NULL_PTR(p) && !klc_find_and_remove_alloc(p)) 
        klc_add_bad_free(p, stack_depth);

    kmem_cache_free(mc, p);
    return;
}

static unsigned long
repl___get_free_pages(gfp_t flags, unsigned int order)
{
    unsigned long ret_val;
    ret_val = __get_free_pages(flags, order);

    if ((void *)ret_val != NULL) {
        klc_add_alloc((const void *)ret_val, 
            (size_t)(PAGE_SIZE << order), stack_depth);
    }

    return ret_val;
}

static unsigned long 
repl_get_zeroed_page(gfp_t gfp_mask)
{
    unsigned long ret_val;
    ret_val = get_zeroed_page(gfp_mask);
    
    if ((void *)ret_val != NULL) {
        klc_add_alloc((const void *)ret_val, 
            (size_t)PAGE_SIZE, stack_depth);
    }

    return ret_val;
}

static void
repl_free_pages(unsigned long addr, unsigned int order)
{
    void *p = (void *)addr;
    if (!ZERO_OR_NULL_PTR(p) && !klc_find_and_remove_alloc(p)) 
        klc_add_bad_free(p, stack_depth);
    
    free_pages(addr, order);
    return;
}

static void
repl___free_pages(struct page *page, unsigned int order)
{
    if (page != NULL) {
        /* If the page is not mapped to the virtual memory,
         * (highmem?) page_address(page) returns NULL.
         */
        const void *p = (const void *)page_address(page);
        if (p != NULL && !klc_find_and_remove_alloc(p)) 
            klc_add_bad_free(p, stack_depth);
    }
    
    __free_pages(page, order);
    return;
}

static struct page *
repl___alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order,
    struct zonelist *zonelist, nodemask_t *nodemask)
{
    struct page *page;
    page = __alloc_pages_nodemask(gfp_mask, order, zonelist, nodemask);
    
    if (page != NULL) {
        const void *p = (const void *)page_address(page);
        if (p != NULL)
            klc_add_alloc(p, (size_t)(PAGE_SIZE << order), stack_depth);
    }
    
    return page;
}

#ifdef KEDR_HAVE_ALLOC_PAGES_CURRENT
static struct page *
repl_alloc_pages_current(gfp_t gfp, unsigned order)
{
    struct page *page;
    page = alloc_pages_current(gfp, order);
    
    if (page != NULL) {
        const void *p = (const void *)page_address(page);
        if (p != NULL) 
            klc_add_alloc(p, (size_t)(PAGE_SIZE << order), stack_depth);
    }
    
    return page;
}
#endif

static void *
repl_alloc_pages_exact(size_t size, gfp_t gfp_mask)
{
    void *ret_val;
    ret_val = alloc_pages_exact(size, gfp_mask);
    
    if (!ZERO_OR_NULL_PTR(ret_val))
        klc_add_alloc(ret_val, size, stack_depth);

    return ret_val;
}

static void 
repl_free_pages_exact(void *virt, size_t size)
{
    if (!ZERO_OR_NULL_PTR(virt) && !klc_find_and_remove_alloc(virt)) 
        klc_add_bad_free(virt, stack_depth);
    
    free_pages_exact(virt, size);
    return;
}

/*********************************************************************
 * "duplicators" group
 *********************************************************************/
static char *
repl_kstrdup(const char *s, gfp_t gfp)
{
    char *ret_val;
    ret_val = kstrdup(s, gfp);
    
    if (ret_val != NULL)
        klc_add_alloc((void *)ret_val, strlen(s) + 1, stack_depth);
    
    return ret_val;
}

static char *
repl_kstrndup(const char *s, size_t max, gfp_t gfp)
{
    char *ret_val;
    ret_val = kstrndup(s, max, gfp);
    
    if (ret_val != NULL)
        klc_add_alloc((void *)ret_val, strnlen(s, max) + 1, stack_depth);
    
    return ret_val;
}

static void *
repl_kmemdup(const void *src, size_t len, gfp_t gfp)
{
    void *ret_val;
    ret_val = kmemdup(src, len, gfp);
    
    if (ret_val != NULL)
        klc_add_alloc(ret_val, len, stack_depth);

    return ret_val;
}

static char *
repl_strndup_user(const char __user *s, long n)
{
    char *ret_val;
    ret_val = strndup_user(s, n);
    
    if (!IS_ERR(ret_val))
        klc_add_alloc((void *)ret_val, strnlen_user(s, n), stack_depth);
    
    return ret_val;
}

static void *
repl_memdup_user(const void __user *src, size_t len)
{
    void *ret_val;
    ret_val = memdup_user(src, len);
    
    if (!IS_ERR(ret_val))
        klc_add_alloc(ret_val, len, stack_depth);
    
    return ret_val;
}

/*********************************************************************
 * "vmalloc" group
 *********************************************************************/
static void *
repl_vmalloc(unsigned long size)
{
    void *ret_val;
    ret_val = vmalloc(size);
    
    if (ret_val != NULL)
        klc_add_alloc(ret_val, size, stack_depth);

    return ret_val;
}

static void *
repl___vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot)
{
    void *ret_val;
    ret_val = __vmalloc(size, gfp_mask, prot);

    if (ret_val != NULL)
        klc_add_alloc(ret_val, size, stack_depth);

    return ret_val;
}

static void *
repl_vmalloc_user(unsigned long size)
{
    void *ret_val;
    ret_val = vmalloc_user(size);

    if (ret_val != NULL)
        klc_add_alloc(ret_val, size, stack_depth);

    return ret_val;
}

static void *
repl_vmalloc_node(unsigned long size, int node)
{
    void *ret_val;
    ret_val = vmalloc_node(size, node);

    if (ret_val != NULL)
        klc_add_alloc(ret_val, size, stack_depth);

    return ret_val;
}

static void *
repl_vmalloc_32(unsigned long size)
{
    void *ret_val;
    ret_val = vmalloc_32(size);

    if (ret_val != NULL)
        klc_add_alloc(ret_val, size, stack_depth);

    return ret_val;
}

static void *
repl_vmalloc_32_user(unsigned long size)
{
    void *ret_val;
    ret_val = vmalloc_32_user(size);

    if (ret_val != NULL)
        klc_add_alloc(ret_val, size, stack_depth);

    return ret_val;
}

static void
repl_vfree(const void *addr)
{
    if (!ZERO_OR_NULL_PTR(addr) && !klc_find_and_remove_alloc(addr)) 
        klc_add_bad_free(addr, stack_depth);

    vfree(addr);
    return;    
}

/*********************************************************************/
/* Names and addresses of the functions of interest */
static void *orig_addrs[] = {
    /* "kmalloc" group */
    (void *)&__kmalloc,
    (void *)&krealloc,
    (void *)&__krealloc,
    (void *)&kfree,
    (void *)&kzfree,

    (void *)&kmem_cache_alloc,
#ifdef KEDR_HAVE_KMCA_NOTRACE 
    (void *)&kmem_cache_alloc_notrace,
#endif
    (void *)&kmem_cache_free,

    (void *)&__get_free_pages,
    (void *)&get_zeroed_page,
    (void *)&free_pages,
    (void *)&__free_pages,
    (void *)&__alloc_pages_nodemask,
#ifdef KEDR_HAVE_ALLOC_PAGES_CURRENT
    (void *)&alloc_pages_current,
#endif
    (void *)&alloc_pages_exact,
    (void *)&free_pages_exact,
    
    /* "duplicators" group */
    (void *)&kstrdup,
    (void *)&kstrndup,
    (void *)&kmemdup,
    (void *)&strndup_user,
    (void *)&memdup_user,
    
    /* "vmalloc" group */
    (void *)&vmalloc,
    (void *)&__vmalloc,
    (void *)&vmalloc_user,
    (void *)&vmalloc_node,
    (void *)&vmalloc_32,
    (void *)&vmalloc_32_user,
    (void *)&vfree,
};

/* Addresses of the replacement functions */
static void *repl_addrs[] = {
    /* "kmalloc" group */
    (void *)&repl___kmalloc,
    (void *)&repl_krealloc,
    (void *)&repl___krealloc,
    (void *)&repl_kfree,
    (void *)&repl_kzfree,

    (void *)&repl_kmem_cache_alloc,
#ifdef KEDR_HAVE_KMCA_NOTRACE 
    (void *)&repl_kmem_cache_alloc_notrace,
#endif
    (void *)&repl_kmem_cache_free,

    (void *)&repl___get_free_pages,
    (void *)&repl_get_zeroed_page,
    (void *)&repl_free_pages,
    (void *)&repl___free_pages,
    (void *)&repl___alloc_pages_nodemask,
#ifdef KEDR_HAVE_ALLOC_PAGES_CURRENT
    (void *)&repl_alloc_pages_current,
#endif
    (void *)&repl_alloc_pages_exact,
    (void *)&repl_free_pages_exact,

    /* "duplicators" group */
    (void *)&repl_kstrdup,
    (void *)&repl_kstrndup,
    (void *)&repl_kmemdup,
    (void *)&repl_strndup_user,
    (void *)&repl_memdup_user,
    
    /* "vmalloc" group */
    (void *)&repl_vmalloc,
    (void *)&repl___vmalloc,
    (void *)&repl_vmalloc_user,
    (void *)&repl_vmalloc_node,
    (void *)&repl_vmalloc_32,
    (void *)&repl_vmalloc_32_user,
    (void *)&repl_vfree,
};

static struct kedr_payload payload = {
    .mod                    = THIS_MODULE,
    .repl_table.orig_addrs  = &orig_addrs[0],
    .repl_table.repl_addrs  = &repl_addrs[0],
    .repl_table.num_addrs   = ARRAY_SIZE(orig_addrs),
    .target_load_callback   = target_load_callback,
    .target_unload_callback = target_unload_callback
};
/*********************************************************************/

static void
payload_cleanup_module(void)
{
    kedr_payload_unregister(&payload);
    klc_output_fini();
    
    KEDR_MSG("[kedr_leak_check] Cleanup complete\n");
    return;
}

static int __init
payload_init_module(void)
{
    int ret = 0;
    
    BUILD_BUG_ON(ARRAY_SIZE(orig_addrs) != 
        ARRAY_SIZE(repl_addrs));

    KEDR_MSG("[kedr_leak_check] Initializing\n");
    
    if (stack_depth == 0 || stack_depth > KEDR_MAX_FRAMES) {
        printk(KERN_ERR "[kedr_leak_check] "
            "Invalid value of 'stack_depth': %u (should be a positive "
            "integer not greater than %u)\n",
            stack_depth,
            KEDR_MAX_FRAMES
        );
        return -EINVAL;
    }
    
    klc_init_mbi_storage();
    
    ret = klc_output_init();
    if (ret != 0)
        return ret;
    
    ret = kedr_payload_register(&payload);
    if (ret != 0) 
        goto fail_reg;
  
    return 0;

fail_reg:
    klc_output_fini();
    return ret;
}

module_init(payload_init_module);
module_exit(payload_cleanup_module);
/*********************************************************************/
