/* klc_output.h
 * Helpers for data output.
 * This provides additional abstraction that allows to output data from 
 * the payload module without directly using printk, trace-related stuff
 * or whatever. The way the data is output is subject to change, this 
 * abstraction helps make these changes local.
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
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>

#include "klc_output.h"

/* ================================================================ */
/* A directory for output files in debugfs. */
struct dentry *dir_klc = NULL;

/* The files in debugfs where the output will go. */
struct dentry *file_leaks = NULL;
struct dentry *file_bad_frees = NULL;
struct dentry *file_stats = NULL;

/* ================================================================ */
/* Output buffer that accumulates strings sent to it by klc_print_string().
 * The buffer grows automatically when necessary.
 * The operations with each such buffer (klc_output_buffer_*(), etc.) 
 * should be performed with the corresponding mutex locked (see 'lock' 
 * field in the structure). 
 *
 * The caller must also ensure that no other operations with a buffer can
 * occur during the creation and descruction of the buffer.
 */
struct klc_output_buffer
{
    /* the buffer itself */
    char *buf; 
    
    /* the size of the buffer */
    size_t size; 
    
    /* length of the data stored (excluding the terminating '\0')*/
    size_t data_len; 
    
    /* the mutex to guard access to the buffer */
    struct mutex *lock; 
};

/* Default size of the buffer. */
#define KLC_OUTPUT_BUFFER_SIZE 1000

/* The mutexes to guard access to the buffers. */
DEFINE_MUTEX(ob_leaks_mutex);
DEFINE_MUTEX(ob_bad_frees_mutex);
DEFINE_MUTEX(ob_other_mutex);

/* Output buffers for each type of output resource. */
struct klc_output_buffer ob_leaks = {
    .buf = NULL,
    .size = 0,
    .data_len = 0,
    .lock = &ob_leaks_mutex
};

struct klc_output_buffer ob_bad_frees = {
    .buf = NULL,
    .size = 0,
    .data_len = 0,
    .lock = &ob_bad_frees_mutex
};

struct klc_output_buffer ob_other = {
    .buf = NULL,
    .size = 0,
    .data_len = 0,
    .lock = &ob_other_mutex
};

/* ================================================================ */
/* Initializes the buffer: allocates memory, etc.
 * Returns 0 on success, a negative error code on failure.
 */
static int
klc_output_buffer_init(struct klc_output_buffer *ob)
{
    BUG_ON(ob == NULL);

    ob->buf = (char *)kmalloc(KLC_OUTPUT_BUFFER_SIZE, GFP_KERNEL);
    if (ob->buf == NULL)
        return -ENOMEM;
    
    ob->buf[0] = 0;
    ob->size = KLC_OUTPUT_BUFFER_SIZE;
    ob->data_len = 0;
    
    return 0;
}

/* Destroys the buffer: releases the memory pointed to by 'ob->buf', etc.
 */
static void
klc_output_buffer_destroy(struct klc_output_buffer *ob)
{
    BUG_ON(ob == NULL);
    
    ob->data_len = 0;
    ob->size = 0;
    kfree(ob->buf);
    return;
}

/* Enlarges the buffer to make it at least 'new_size' bytes in size.
 * If 'new_size' is less than or equal to 'ob->size', the function does 
 * nothing.
 * If there is not enough memory, the function outputs an error to 
 * the system log, leaves the buffer intact and returns -ENOMEM.
 * 0 is returned in case of success.
 */
static int
klc_output_buffer_resize(struct klc_output_buffer *ob, size_t new_size)
{
    size_t size;
    void *p;
    BUG_ON(ob == NULL);
    
    if (ob->size >= new_size)
        return 0;
    
    size = ob->size;
    do {
         size *= 2;
    } while (size < new_size);
    
    p = krealloc(ob->buf, size, GFP_KERNEL);
    if (p == NULL) {
    /* [NB] If krealloc fails to allocate memory, it should leave the buffer
     * intact, so its previous contents could still be used.
     */
        printk(KERN_ERR "[kedr_leak_check] klc_output_buffer_resize: "
            "not enough memory to resize the output buffer to %zu bytes\n",
            size);
        return -ENOMEM;
    }
    
    ob->buf = p;
    ob->size = size;
    
    return 0;
}

/* Appends the specified string to the buffer, enlarging the latter if 
 * necessary with klc_output_buffer_resize().
 */
static void
klc_output_buffer_append(struct klc_output_buffer *ob, const char *s)
{
    size_t len;
    int ret;
    
    BUG_ON(ob == NULL);
    BUG_ON(ob->buf[ob->data_len] != 0);
    BUG_ON(s == NULL);
    
    len = strlen(s);
    if (len == 0)   /* nothing to do */
        return;
    
    /* make sure the buffer is large enough */
    ret = klc_output_buffer_resize(ob, ob->data_len + len + 1);
    if (ret != 0)
        return; /* the error has already been reported */
    
    strncpy(&(ob->buf[ob->data_len]), s, len + 1);
    ob->data_len += len;
    return;
}
/* ================================================================ */

/* A convenience macro to define variable of type struct file_operations
 * for a read only file in debugfs associated with the specified output
 * buffer.
 * 
 * __fops - the name of the variable
 * __ob - pointer to the output buffer (struct klc_output_buffer *)
 */
#define KLC_DEFINE_FOPS_RO(__fops, __ob)                                \
static int __fops ## _open(struct inode *inode, struct file *filp)      \
{                                                                       \
    BUILD_BUG_ON(sizeof(*(__ob)) != sizeof(struct klc_output_buffer));  \
    filp->private_data = (void *)(__ob);                                \
    return nonseekable_open(inode, filp);                               \
}                                                                       \
static const struct file_operations __fops = {                          \
    .owner      = THIS_MODULE,                                          \
    .open       = __fops ## _open,                                      \
    .release    = klc_release_common,                                   \
    .read       = klc_read_common,                                      \
};

/* Helpers for file operations common to all read-only files in this 
 * example. */
static int 
klc_release_common(struct inode *inode, struct file *filp)
{
    filp->private_data = NULL;
    /* nothing more to do: open() did not allocate any resources */
    return 0;
}

static ssize_t 
klc_read_common(struct file *filp, char __user *buf, size_t count,
    loff_t *f_pos)
{
    ssize_t ret = 0;
    size_t data_len;
    loff_t pos = *f_pos;
    struct klc_output_buffer *ob = 
        (struct klc_output_buffer *)filp->private_data;
        
    if (ob == NULL) 
        return -EINVAL;
    
    if (mutex_lock_killable(ob->lock) != 0)
	{
        printk(KERN_WARNING "[kedr_leak_check] klc_read_common: "
            "got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
    
    data_len = ob->data_len;
    
    /* Reading outside of the data buffer is not allowed */
    if ((pos < 0) || (pos > data_len)) {
        ret = -EINVAL;
        goto out;
    }
    
    /* EOF reached or 0 bytes requested */
    if ((count == 0) || (pos == data_len)) {
        ret = 0; 
        goto out;
    }
    
    if (pos + count > data_len) 
        count = data_len - pos;
    if (copy_to_user(buf, &(ob->buf[pos]), count) != 0) {
        ret = -EFAULT;
        goto out;
    }

    mutex_unlock(ob->lock);
    
    *f_pos += count;
    return count;
    
out:
    mutex_unlock(ob->lock);
    return ret;
}

/* Definitions of file_operations structures for the files in debugfs.
 */
KLC_DEFINE_FOPS_RO(fops_leaks_ro, &ob_leaks);
KLC_DEFINE_FOPS_RO(fops_bad_frees_ro, &ob_bad_frees);
KLC_DEFINE_FOPS_RO(fops_stats_ro, &ob_other);
/* ================================================================ */

static void
klc_remove_debugfs_files(void)
{
    if (file_leaks      != NULL) debugfs_remove(file_leaks);
    if (file_bad_frees  != NULL) debugfs_remove(file_bad_frees);
    if (file_stats      != NULL) debugfs_remove(file_stats);
    return;
}
    
static int
klc_create_debugfs_files(void)
{
/* [NB] We do not check here if debugfs is supported because this is done 
 * when creating the directory for these files ('dir_klc').
 */
    file_leaks = debugfs_create_file("possible_leaks", S_IRUGO,
        dir_klc, NULL, &fops_leaks_ro);
    if (file_leaks == NULL) 
        goto fail;
    
    file_bad_frees = debugfs_create_file("unallocated_frees", S_IRUGO,
        dir_klc, NULL, &fops_bad_frees_ro);
    if (file_bad_frees == NULL) 
        goto fail;
    
    file_stats = debugfs_create_file("info", S_IRUGO,
        dir_klc, NULL, &fops_stats_ro);
    if (file_stats == NULL) 
        goto fail;

    return 0;

fail:
    printk(KERN_ERR "[kedr_leak_check] "
        "failed to create output files in debugfs\n"
    );
    klc_remove_debugfs_files();
    return -EINVAL;    
}

/* ================================================================ */
void
klc_print_string(enum klc_output_type output_type, const char *s)
{
    struct klc_output_buffer *ob = NULL;
    
    BUG_ON(s == NULL);

    switch (output_type) {
    case KLC_UNFREED_ALLOC:
        ob = &ob_leaks;
        break;
    case KLC_UNALLOCATED_FREE: 
        ob = &ob_bad_frees;
        break;
    case KLC_OTHER:
        ob = &ob_other;
        break;
    default:
        printk(KERN_WARNING "[kedr_leak_check] unknown output type: %d\n", 
            (int)output_type);
        return;
    }
    BUG_ON(ob->buf == NULL); 
    
    if (mutex_lock_killable(ob->lock) != 0)
	{
        printk(KERN_WARNING "[kedr_leak_check] klc_print_string: "
            "got a signal while trying to acquire a mutex.\n");
		return;
	}
    
    klc_output_buffer_append(ob, s);
    klc_output_buffer_append(ob, "\n");

    mutex_unlock(ob->lock);
    return;
}

void
klc_print_stack_trace(enum klc_output_type output_type, 
    unsigned long *stack_entries, unsigned int num_entries)
{
    static const char* fmt = "[<%p>] %pS";
    
    /* This is just to pass a buffer of known size to the first call
     * to snprintf() to determine the length of the string to which
     * the data will be converted. 
     */
    char one_char[1];
    char *buf = NULL;
    int len;
    unsigned int i;
    
    BUG_ON(stack_entries == NULL);
    
    if (num_entries == 0)
        return;
    
    for (i = 0; i < num_entries; ++i) {
        len = snprintf(&one_char[0], 1, fmt, 
            (void *)stack_entries[i], (void *)stack_entries[i]);
        buf = (char*)kmalloc(len + 1, GFP_KERNEL);
        if (buf != NULL) {
            snprintf(buf, len + 1, fmt, 
                (void *)stack_entries[i], (void *)stack_entries[i]);
            klc_print_string(output_type, buf);
            kfree(buf);
        } else { 
            printk(KERN_ERR "[kedr_leak_check] klc_print_stack_trace: "
                "not enough memory to prepare a message of size %d\n",
                len);
        }
    }
    return;    
}

void
klc_print_target_module_info(struct module *target_module)
{
    static const char* fmt = 
"Target module: \"%s\", init area at 0x%p, core area at 0x%p";
    
    char one_char[1];
    char *buf = NULL;
    int len;
    const char *name;
    
    BUG_ON(target_module == NULL);
    name = module_name(target_module);
    
    len = snprintf(&one_char[0], 1, fmt, name, 
        target_module->module_init, target_module->module_core);
    buf = (char*)kmalloc(len + 1, GFP_KERNEL);
    if (buf == NULL) {
        printk(KERN_ERR "[kedr_leak_check] klc_print_target_module_info: "
            "not enough memory to prepare a message of size %d\n",
            len);
    }
    snprintf(buf, len + 1, fmt, name, 
        target_module->module_init, target_module->module_core);
    klc_print_string(KLC_OTHER, buf);
    kfree(buf);
    return;
}

void 
klc_print_alloc_info(struct klc_memblock_info *alloc_info)
{
    static const char* fmt = 
        "Block at 0x%p, size: %zu; stack trace of the allocation:";
    
    char one_char[1];
    char *buf = NULL;
    int len;
    
    BUG_ON(alloc_info == NULL);
    
    len = snprintf(&one_char[0], 1, fmt, alloc_info->block, 
        alloc_info->size);
    buf = (char*)kmalloc(len + 1, GFP_KERNEL);
    if (buf == NULL) {
        printk(KERN_ERR "[kedr_leak_check] klc_print_alloc_info: "
            "not enough memory to prepare a message of size %d\n",
            len);
    }
    snprintf(buf, len + 1, fmt, alloc_info->block, alloc_info->size);
    klc_print_string(KLC_UNFREED_ALLOC, buf);
    kfree(buf);
    
    klc_print_stack_trace(KLC_UNFREED_ALLOC, 
        &(alloc_info->stack_entries[0]), alloc_info->num_entries);
    
    klc_print_string(KLC_UNFREED_ALLOC, 
        "----------------------------------------"); /* separator */
    return;
}

void 
klc_print_dealloc_info(struct klc_memblock_info *dealloc_info)
{
    static const char* fmt = 
        "Block at 0x%p; stack trace of the deallocation:";
    
    char one_char[1];
    char *buf = NULL;
    int len;
 
    BUG_ON(dealloc_info == NULL);
    
    len = snprintf(&one_char[0], 1, fmt, dealloc_info->block);
    buf = (char*)kmalloc(len + 1, GFP_KERNEL);
    if (buf == NULL) {
        printk(KERN_ERR "[kedr_leak_check] klc_print_dealloc_info: "
            "not enough memory to prepare a message of size %d\n",
            len);
    }
    snprintf(buf, len + 1, fmt, dealloc_info->block);
    klc_print_string(KLC_UNALLOCATED_FREE, buf);
    kfree(buf);
    
    klc_print_stack_trace(KLC_UNALLOCATED_FREE, 
        &(dealloc_info->stack_entries[0]), dealloc_info->num_entries);
    
    klc_print_string(KLC_UNALLOCATED_FREE, 
        "----------------------------------------"); /* separator */
    return;
}

/* A helper function to print an unsigned 64-bit value using the specified
 * format. The format must contain "%llu", "%llx" or the like.
 */
static void 
klc_print_u64(enum klc_output_type output_type, u64 data, const char *fmt)
{
    char one_char[1];
    char *buf = NULL;
    int len;
    
    BUG_ON(fmt == NULL);
    
    len = snprintf(&one_char[0], 1, fmt, data);
    buf = (char*)kmalloc(len + 1, GFP_KERNEL);
    if (buf == NULL) {
        printk(KERN_ERR "[kedr_leak_check] klc_print_u64: "
            "not enough memory to prepare a message of size %d\n",
            len);
    }
    snprintf(buf, len + 1, fmt, data);
    klc_print_string(output_type, buf);
    kfree(buf);
    
    return;    
}

void
klc_print_totals(u64 total_allocs, u64 total_leaks, u64 total_bad_frees)
{
    klc_print_u64(KLC_OTHER, total_allocs, 
        "Memory allocations: %llu");
    klc_print_u64(KLC_OTHER, total_leaks,
        "Possible leaks: %llu");
    klc_print_u64(KLC_OTHER, total_bad_frees,
        "Unallocated frees: %llu");
    return;
}
/* ================================================================ */

int 
klc_output_init(void)
{
    int ret = 0;
    
    /* Create output buffers */
    ret = klc_output_buffer_init(&ob_leaks);
    if (ret != 0)
        goto fail_ob;
    
    ret = klc_output_buffer_init(&ob_bad_frees);
    if (ret != 0)
        goto fail_ob;
    
    ret = klc_output_buffer_init(&ob_other);
    if (ret != 0)
        goto fail_ob;
    
    /* Create a directory in debugfs */
    dir_klc = debugfs_create_dir("kedr_leak_check", NULL);
    if (IS_ERR(dir_klc)) {
        printk(KERN_ERR "[kedr_leak_check] debugfs is not supported\n");
        dir_klc = NULL;
        ret = -ENODEV;
        goto fail_ob;
    }
    
    if (dir_klc == NULL) {
        printk(KERN_ERR "[kedr_leak_check] "
            "failed to create a directory in debugfs\n");
        ret = -EINVAL;
        goto fail_ob;
    }
    
    /* Create output files */
    ret = klc_create_debugfs_files();
    if (ret != 0)
        goto fail_files;

    return 0;

fail_files:
    klc_remove_debugfs_files();
    debugfs_remove(dir_klc);
    
fail_ob:
    klc_output_buffer_destroy(&ob_leaks);
    klc_output_buffer_destroy(&ob_bad_frees);
    klc_output_buffer_destroy(&ob_other);
    return ret;
}

void
klc_output_clear(void)
{
    /* "Clear" the data without actually releasing memory. 
     * No need for locking as the caller ensures that no output may 
     * interfere.
     */
    ob_leaks.data_len = 0;
    ob_leaks.buf[0] = 0;
    
    ob_bad_frees.data_len = 0;
    ob_bad_frees.buf[0] = 0;
    
    ob_other.data_len = 0;
    ob_other.buf[0] = 0;
    return;
}

void
klc_output_fini(void)
{
    klc_remove_debugfs_files();
    if (dir_klc != NULL) 
        debugfs_remove(dir_klc);
    
    klc_output_buffer_destroy(&ob_leaks);
    klc_output_buffer_destroy(&ob_bad_frees);
    klc_output_buffer_destroy(&ob_other);
    return;
}
/* ================================================================ */
