/*
 * counters.h - utility declarations for "kedr_counters" module.
 */
#ifndef COUNTERS_H_1250_INCLUDED
#define COUNTERS_H_1250_INCLUDED

#include <linux/debugfs.h>

/* A convenience macro to define variable of type struct file_operations
 * for a read only file in debugfs that displays a value of a single 
 * parameter.
 * (Modeled in a similar way as DEFINE_SIMPLE_ATTRIBUTE() in linux/fs.h.)
 * 
 * __fops - the name of the variable
 * __type - type of the value to be displayed in the file
 * __get_copy_func - name of the function that creates a copy of the value
 *      This function should be provided by the caller.
 *      All operations with the file affect only this copy rather than the 
 *      original value. The function must have the following type:
 *      int (*)(__type **)
 *      The function must return 0 on success and a negative error code 
 *      otherwise. The pointer to the copy of the value should be returned 
 *      via the argument of the function. The pointer to that copy should 
 *      be allocated with kmalloc (or a compatible function). The 
 *      implementation of file operations will call kfree() for it when it
 *      is no longer needed.
 * __fmt - snprintf-style format string to be used to convert the value to 
 *      a string, for example, "%llu\n" or the like.
 */
#undef COUNTERS_DEFINE_FOPS_RO
#define COUNTERS_DEFINE_FOPS_RO(__fops, __type, __get_copy_func, __fmt) \
static int __fops ## _open(struct inode *inode, struct file *filp)      \
{                                                                       \
    char *buf = NULL;                                                   \
    __type *pdata = NULL; /* The data to be shown to the reader(s) */   \
    int error = 0;                                                      \
    int dataLen = 0;                                                    \
                                                                        \
    error = __get_copy_func(&pdata);                                    \
    if (error) return error;                                            \
    BUG_ON(pdata == NULL);                                              \
                                                                        \
    dataLen = snprintf(NULL, 0, __fmt, *pdata);                         \
    buf = (char*)kmalloc(dataLen + 1, GFP_KERNEL);                      \
    if (buf == NULL) {                                                  \
        kfree(pdata);                                                   \
        return -ENOMEM;                                                 \
    }                                                                   \
                                                                        \
    snprintf(buf, dataLen + 1, __fmt, *pdata);                          \
    kfree(pdata);                                                       \
    filp->private_data = (void *)buf;                                   \
                                                                        \
    return nonseekable_open(inode, filp);                               \
}                                                                       \
static const struct file_operations __fops = {                          \
    .owner      = THIS_MODULE,                                          \
    .open       = __fops ## _open,                                      \
    .release    = counters_release_common,                              \
    .read       = counters_read_common,                                 \
};

/* File operations common for all read-only files in this example */
int 
counters_release_common(struct inode *inode, struct file *filp);

ssize_t 
counters_read_common(struct file *filp, char __user *buf, size_t count,
    loff_t *f_pos);

/* ================================================================ */
#endif /* COUNTERS_H_1250_INCLUDED */
