/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Copyright (C) 2010-2012, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <kedr/control_file/control_file.h>

#include <linux/fs.h>

#include <linux/kernel.h> /* pr_err */

#include <linux/slab.h> /* kmalloc */
#include <linux/string.h> /* string relating operations*/

#include <linux/uaccess.h> /* copy_*_user functions */


/*
 * 'Renderer' of the string.
 *
 * 'str' is initial string, 
 *
 * Return allocated transformation.
 */

static char*
read_transform_str(const char* str)
{
    char* result;
    size_t len_initial = strlen(str);
    result = kmalloc(len_initial + 2, GFP_KERNEL);
    if(result == NULL) return result;
    memcpy(result, str, len_initial);
    result[len_initial] = '\n';
    result[len_initial + 1] = '\0';
    return result;
}

int control_file_open_wrapper(struct inode *inode, struct file *filp,
    char* (*get_str)(struct inode* inode))
{
    if((filp->f_mode & FMODE_READ) && get_str)
    {
        char* str = get_str(inode);
        if(str == NULL) return -EINVAL;
        filp->private_data = read_transform_str(str);
        kfree(str);
        if(filp->private_data == NULL) return -ENOMEM;
    }
    return nonseekable_open(inode, filp);
}

ssize_t control_file_write_wrapper(struct file *filp,
    const char __user *buf, size_t count, loff_t * f_pos,
    int (*set_str)(const char* str, struct inode *inode))
{
    char* str;
    int error;

    if(set_str == NULL) return -EINVAL;//writing is not supported

    if(count == 0)
    {
        pr_err("write: 'count' shouldn't be 0.");
        return -EINVAL;
    }

    /*
     * Feature of control files.
     *
     * Because writing to such files is really command to the module to do something,
     * and successive reading from this file return total effect of this command.
     * it is meaningless to process writing not from the start.
     *
     * In other words, writing always affect to the global content of the file.
     */
    if(*f_pos != 0)
    {
        pr_err("Partial rewriting is not allowed.");
        return -EINVAL;
    }
    //Allocate buffer for writing value - for its preprocessing.
    str = kmalloc(count + 1, GFP_KERNEL);
    if(str == NULL)
    {
        pr_err("Cannot allocate string.");
        return -ENOMEM;
    }

    if(copy_from_user(str, buf, count) != 0)
    {
        pr_err("copy_from_user return error.");
        kfree(str);
        return -EFAULT;
    }
    // For case, when one try to write not null-terminated sequence of bytes,
    // or omit terminated null-character.
    str[count] = '\0';

    /*
     * Usually, writing to the control file is performed via 'echo' command,
     * which append new-line symbol to the writing string.
     *
     * Because, this symbol is usually not needed, we trim it.
     */
    if(str[count - 1] == '\n') str[count - 1] = '\0';

    error = set_str(str, filp->f_dentry->d_inode);
    kfree(str);
    return error ? error : count;
}


ssize_t control_file_read_common (struct file *filp,
    char __user *buf, size_t count, loff_t *f_pos)
{
    size_t size;
    char* str = filp->private_data;
    
    if(str == NULL) return -EINVAL;//reading is not supported

    size = strlen(str) + 1;//include terminating '\0'
    
    //whether position out of range
    if((*f_pos < 0) || (*f_pos > size)) return -EINVAL;
    if(*f_pos == size) return 0;// eof
    //If need, correct 'count'
    if(count + *f_pos > size)
        count = size - *f_pos;

    if(copy_to_user(buf, str + *f_pos, count) != 0)
        return -EFAULT;

    *f_pos += count;
    return count;
}

int control_file_release_common (struct inode *inode, struct file *filp)
{
    kfree(filp->private_data);
    return 0;
}
