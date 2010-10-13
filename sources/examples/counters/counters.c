#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include <kedr/base/common.h>

#include "counters.h"

/* ================================================================ */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ================================================================ */

/* A directory in debugfs to contain the files that will represent 
 * the counters. */
struct dentry *dir_counters = NULL;

/* The counters, the spinlocks to protect them and the files in debugfs
 * to represent these counters.
 */
/* Number of memory allocation attempts */
u64 cnt_alloc_total = 0;
DEFINE_SPINLOCK(spinlock_alloc_total);
struct dentry *file_alloc_total = NULL;

/* Number of failed memory allocation attempts  */
u64 cnt_alloc_failed = 0;
DEFINE_SPINLOCK(spinlock_alloc_failed);
struct dentry *file_alloc_failed = NULL;

/* Max. chunk size requiested in an allocation attempt */
size_t cnt_alloc_max_size = 0;
DEFINE_SPINLOCK(spinlock_alloc_max_size);
struct dentry *file_alloc_max_size = NULL;

/* Number of successful mutex acquisitions */
u64 cnt_mutex_locks = 0;
DEFINE_SPINLOCK(spinlock_mutex_locks);
struct dentry *file_mutex_locks = NULL;

/* Mutex acquisitions minus mutex releases */
s64 cnt_mutex_balance = 0;
DEFINE_SPINLOCK(spinlock_mutex_balance);
struct dentry *file_mutex_balance = NULL;

/* ================================================================ */
/* 
 * Copying functions for the counters. 
 * Each function creates a copy of the counter and returns a pointer to 
 * the copy in *ppdata.
 * The caller is responsible to call kfree() for that pointer to free 
 * the memory occupied by the copy when it is no longer needed.
 * 
 * Each function returns 0 if OK, negative error code otherwise.
 */

/* A helper macro to define copying functions for the counters.
 * __cnt_name - the name of the counter (without "cnt_" prefix)
 * __numeric_type - the type of the counter
 */
#undef COUNTERS_DEFINE_COPY_FUNCTION
#define COUNTERS_DEFINE_COPY_FUNCTION(__cnt_name, __numeric_type)       \
static int get_copy_ ## __cnt_name(__numeric_type **ppdata)             \
{                                                                       \
    unsigned long flags;                                                \
    BUG_ON(ppdata == NULL);                                             \
                                                                        \
    *ppdata =                                                           \
        (__numeric_type *)kmalloc(sizeof(__numeric_type), GFP_KERNEL);  \
    if (*ppdata == NULL) return -ENOMEM;                                \
                                                                        \
    spin_lock_irqsave(&spinlock_ ## __cnt_name, flags);                 \
    *(*ppdata) = cnt_ ## __cnt_name; /* Copy the data */                \
    spin_unlock_irqrestore(&spinlock_ ## __cnt_name, flags);            \
    return 0;                                                           \
}

COUNTERS_DEFINE_COPY_FUNCTION(alloc_total, u64);
COUNTERS_DEFINE_COPY_FUNCTION(alloc_failed, u64);
COUNTERS_DEFINE_COPY_FUNCTION(alloc_max_size, size_t);
COUNTERS_DEFINE_COPY_FUNCTION(mutex_locks, u64);
COUNTERS_DEFINE_COPY_FUNCTION(mutex_balance, s64);

/* ================================================================ */
/* Using the helper to define file_operations structures */
COUNTERS_DEFINE_FOPS_RO(fops_alloc_total_ro, 
    u64, get_copy_alloc_total, 
    "Memory allocation attempts: %llu\n");
COUNTERS_DEFINE_FOPS_RO(fops_alloc_failed_ro,
    u64, get_copy_alloc_failed, 
    "Memory allocation attempts failed: %llu\n");
COUNTERS_DEFINE_FOPS_RO(fops_alloc_max_size_ro,
    size_t, get_copy_alloc_max_size, 
    "Maximum size of a memory chunk requested: %zu\n");
COUNTERS_DEFINE_FOPS_RO(fops_mutex_locks_ro,
    u64, get_copy_mutex_locks, 
    "Mutex acquisitions: %llu\n");
COUNTERS_DEFINE_FOPS_RO(fops_mutex_balance_ro,
    s64, get_copy_mutex_balance, 
    "Mutex balance: %lld\n");

/* ================================================================ */
static void
remove_debugfs_files(void)
{
    if (file_alloc_total    != NULL) debugfs_remove(file_alloc_total);
    if (file_alloc_failed   != NULL) debugfs_remove(file_alloc_failed);
    if (file_alloc_max_size != NULL) debugfs_remove(file_alloc_max_size);
    if (file_mutex_locks    != NULL) debugfs_remove(file_mutex_locks);
    if (file_mutex_balance  != NULL) debugfs_remove(file_mutex_balance);
    return;
}
    
static int
create_debugfs_files(void)
{
    file_alloc_total = debugfs_create_file("alloc_total", S_IRUGO,
        dir_counters, NULL, &fops_alloc_total_ro);
    if (file_alloc_total == NULL) goto fail;
    
    file_alloc_failed = debugfs_create_file("alloc_failed", S_IRUGO,
        dir_counters, NULL, &fops_alloc_failed_ro);
    if (file_alloc_failed == NULL) goto fail;
    
    file_alloc_max_size = debugfs_create_file("alloc_max_size", S_IRUGO,
        dir_counters, NULL, &fops_alloc_max_size_ro);
    if (file_alloc_max_size == NULL) goto fail;
    
    file_mutex_locks = debugfs_create_file("mutex_locks", S_IRUGO,
        dir_counters, NULL, &fops_mutex_locks_ro);
    if (file_mutex_locks == NULL) goto fail;
    
    file_mutex_balance = debugfs_create_file("mutex_balance", S_IRUGO,
        dir_counters, NULL, &fops_mutex_balance_ro);
    if (file_mutex_balance == NULL) goto fail;

    return 0;

fail:
    printk(KERN_ERR "[counters] "
        "failed to create file for a counter in debugfs\n"
    );
    remove_debugfs_files();
    return -EINVAL;    
}

/* ================================================================ */
static int __init
counters_init(void)
{
    int ret = 0;
    
    dir_counters = debugfs_create_dir("kedr_counters_example", NULL);
    if (IS_ERR(dir_counters)) {
        printk(KERN_ERR "[counters] debugfs is not supported\n");
        return -ENODEV;
    }
    
    if (dir_counters == NULL) {
        printk(KERN_ERR 
            "[counters] failed to create a directory in debugfs\n");
        return -EINVAL;
    }
    
    ret = create_debugfs_files();
    if (ret < 0) {
        debugfs_remove(dir_counters);
        return ret;
    }
    
    /*  int ret = kedr_payload_register(&counters_payload);
    if (ret)
    {
        printk(KERN_ERR "[counters] failed to register payload module.\n");
        TODO: clean debugfs (both dir and files)
        remove_debugfs_files();
        debugfs_remove(dir_counters);
        return ret;
    }
    */
    return 0;
}

static void
counters_exit(void)
{
    /*kedr_payload_unregister(&counters_payload);*/
    remove_debugfs_files();
    debugfs_remove(dir_counters);
    return;
}

module_init(counters_init);
module_exit(counters_exit);
/* ================================================================ */

/* Helpers for file operations common to all read-only files in this 
 * example. */
int 
counters_release_common(struct inode *inode, struct file *filp)
{
	kfree(filp->private_data);
	return 0;
}

ssize_t 
counters_read_common(struct file *filp, char __user *buf, size_t count,
    loff_t *f_pos)
{
	size_t dataLen;
	loff_t pos = *f_pos;
	const char *data = (const char *)filp->private_data;
	
	if (data == NULL) return -EINVAL;
	dataLen = strlen(data) + 1;
	
	/* Reading outside of the data buffer is not allowed */
	if ((pos < 0) || (pos > dataLen)) return -EINVAL;
	
	/* EOF reached or 0 bytes requested */
	if ((count == 0) || (pos == dataLen)) return 0;
	
	if (pos + count > dataLen) count = dataLen - pos;
	if (copy_to_user(buf, &data[pos], count) != 0)
        return -EFAULT;
    
    *f_pos += count;
    return count;
}
/* ================================================================ */
