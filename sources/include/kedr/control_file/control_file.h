#ifndef CONTROL_FILE_H
#define CONTROL_FILE_H

#include <linux/fs.h>

#include <linux/module.h>

//This functions should be used only via macro
int control_file_open_wrapper(struct inode *inode, struct file *filp,
    char* (*get_str)(struct inode* inode));

ssize_t control_file_write_wrapper(struct file *filp,
    const char __user *buf, size_t count, loff_t * f_pos,
    int (*set_str)(const char* str, struct inode *inode));

ssize_t control_file_read_common (struct file *filp,
    char __user *buf, size_t count, loff_t *f_pos);

int control_file_release_common (struct inode *inode, struct file *filp);

/*
 * Macro for create file operations, which
 * may be used for control file.
 *
 * 'name' - name of file operations of type static file_operations,
 *
 * 'get_str' - function of type char* (*get_str)(struct inode *inode),
 *      which should return allocated string, which will be result of reading from file
 *
 * 'set_str' - function of type int (*set_str)(const char* str, struct inode *inode),
 *      which should perform actions in correspondence to the string, writting to file.
 *      It should return 0 on success or negative error code.
 *
 * Each of function may be NULL, which mean that read/write operation shouldn't be supported.
 */
#define CONTROL_FILE_OPS(name, get_str, set_str)                            \
static int name##_open(struct inode *inode, struct file *filp)              \
{                                                                           \
    return control_file_open_wrapper(inode, filp, get_str);                 \
}                                                                           \
static ssize_t name##_write(struct file *filp,                              \
    const char __user *buf, size_t count, loff_t * f_pos)                   \
{                                                                           \
    return control_file_write_wrapper(filp, buf, count, f_pos, set_str);    \
}                                                                           \
static struct file_operations name = {                                      \
    .owner = THIS_MODULE,                                                   \
    .open = name##_open,                                                    \
    .release = control_file_release_common,                                 \
    .read = control_file_read_common,                                       \
    .write = name##_write                                                   \
}

#endif