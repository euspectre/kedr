/* klc_output.c - helpers for data output.
 * This provides additional abstraction that allows to output data from 
 * the payload module. */

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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#include "leak_check_impl.h"
#include "klc_output.h"
/* ====================================================================== */

/* Main directory for LeakCheck in debugfs. */
static struct dentry *dir_klc_main = NULL;
/* ====================================================================== */

/* A separator for the records in the report files. */
static const char *sep = "----------------------------------------";
/* ====================================================================== */

/* Types of information that can be output.
 * The point is, different types of information can be output to different
 * places or distinguished in some other way. */
enum klc_output_type {
	/* possible leaks */
	KLC_UNFREED_ALLOC,
	
	/* bad frees */
	KLC_BAD_FREE,
	
	/* other info: parameters of the target module, totals, ... */
	KLC_OTHER
};

/* Outputs a string pointed to by 's' taking type of this information into.
 * account ('output_type').
 * The implementation defines where the string will be output and how 
 * different kinds of information will be distinguished.
 * This function is a basic block for the functions that output particular
 * data structures.
 *
 * A newline will be added at the end automatically.
 *
 * This function cannot be used in atomic context. */
void
klc_print_string(struct kedr_lc_output *output, 
	enum klc_output_type output_type, const char *s);

/* Outputs first 'num_entries' elements of 'stack_entries' array as a stack
 * trace. 
 * 
 * This function cannot be used in atomic context. */
void
klc_print_stack_trace(struct kedr_lc_output *output, 
	enum klc_output_type output_type, 
	struct stack_entry **stack_entries, unsigned int num_entries);
/* ====================================================================== */

/* An output buffer that accumulates strings sent to it by 
 * klc_print_string().
 * The buffer grows automatically when necessary.
 * The operations with each such buffer (klc_output_buffer_*(), etc.) 
 * should be performed with the corresponding mutex locked (see 'lock' 
 * field in the structure). 
 *
 * The caller must also ensure that no other operations with a buffer can
 * occur during the creation and descruction of the buffer. */
struct klc_output_buffer
{
	/* the buffer itself */
	char *buf; 
	
	/* size of the buffer */
	size_t size; 
	
	/* length of the data stored (excluding the terminating '\0')*/
	size_t data_len; 
	
	/* a mutex to guard access to the buffer */
	struct mutex lock; 
};

/* Default size of the buffer. */
#define KLC_OUTPUT_BUFFER_SIZE 1000

/* The structure for the output objects. */
struct kedr_lc_output
{
	/* The files in debugfs where the output will go. */
	struct dentry *file_leaks;
	struct dentry *file_bad_frees;
	struct dentry *file_stats;

	/* The file to force LeakCheck to flush the results obtained so far
	 * to make them available in the output files. */
	struct dentry *file_flush;

	/* The file to force LeakCheck to clear the information about memory
	 * allocations and deallocations collected so far. */
	struct dentry *file_clear;
	
	/* Output buffers for each type of output resource. */
	struct klc_output_buffer ob_leaks;
	struct klc_output_buffer ob_bad_frees;
	struct klc_output_buffer ob_other;
};
/* ====================================================================== */

/* Initializes the buffer: allocates memory, etc. */
static int
klc_output_buffer_init(struct klc_output_buffer *ob)
{
	BUG_ON(ob == NULL);

	ob->buf = vmalloc(KLC_OUTPUT_BUFFER_SIZE);
	if (ob->buf == NULL)
		return -ENOMEM;
		
	ob->buf[0] = '\0';
	ob->size = KLC_OUTPUT_BUFFER_SIZE;
	ob->data_len = 0;
	
	mutex_init(&ob->lock);
	return 0;
}

/* Frees the memory allocated for the buffer. */
static void
klc_output_buffer_cleanup(struct klc_output_buffer *ob)
{
	BUG_ON(ob == NULL);

	mutex_destroy(&ob->lock);

	ob->data_len = 0;
	ob->size = 0;
	vfree(ob->buf);
	ob->buf = NULL;
}

/* Enlarges the buffer to make it at least 'new_size' bytes in size.
 * If 'new_size' is less than or equal to 'ob->size', the function does 
 * nothing.
 * If there is not enough memory, the function outputs an error to 
 * the system log, leaves the buffer intact and returns -ENOMEM.
 * 0 is returned in case of success. */
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
	
	p = vmalloc((size_t)size);
	if (p == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX 
	"klc_output_buffer_resize(): "
	"not enough memory to resize the output buffer to %zu bytes\n",
			size);
		return -ENOMEM;
	}
	
	memcpy(p, ob->buf, ob->size); 
	vfree(ob->buf);
	ob->buf = p;
	ob->buf[ob->data_len] = '\0';
	ob->size = size;
	return 0;
}

/* Appends the specified string to the buffer, enlarging the latter if 
 * necessary with klc_output_buffer_resize(). */
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
}
/* ====================================================================== */

/* Helpers for file operations common to all read-only files in debugfs 
 * associated with the specified output buffers. The address of the 
 * struct klc_output_buffer instance should be passed as 'data' to 
 * debugfs_create_file(). It will then be available in 'inode->i_private'.*/
static int 
klc_open_common(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return nonseekable_open(inode, filp);
}

static int 
klc_release_common(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
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
	
	if (mutex_lock_killable(&ob->lock) != 0)
	{
		pr_warn(KEDR_LC_MSG_PREFIX "klc_read_common(): "
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

	mutex_unlock(&ob->lock);
	
	*f_pos += count;
	return count;
	
out:
	mutex_unlock(&ob->lock);
	return ret;
}

/* A single set of file operations is used to maintain the files in debugfs
 * created for each output object. */
static const struct file_operations klc_fops = {
	.owner      = THIS_MODULE,
	.open       = klc_open_common,
	.release    = klc_release_common,
	.read       = klc_read_common,
};

static int
klc_flush_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return nonseekable_open(inode, filp);
}

static int
klc_flush_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static ssize_t
klc_flush_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
	kedr_lc_flush_results(filp->private_data);
	*f_pos += count; /* as if we have written something */
	return count;
}

static const struct file_operations klc_flush_ops = {
	.owner = THIS_MODULE,
	.open = klc_flush_open,
	.release = klc_flush_release,
	.write = klc_flush_write,
};

static int
klc_clear_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return nonseekable_open(inode, filp);
}

static int
klc_clear_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static ssize_t
klc_clear_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
	kedr_lc_clear(filp->private_data);
	*f_pos += count; /* as if we have written something */
	return count;
}

static const struct file_operations klc_clear_ops = {
	.owner = THIS_MODULE,
	.open = klc_clear_open,
	.release = klc_clear_release,
	.write = klc_clear_write,
};
/* ====================================================================== */

static void
klc_remove_debugfs_files(struct kedr_lc_output *output)
{
	BUG_ON(output == NULL);
	if (output->file_leaks != NULL) {
		debugfs_remove(output->file_leaks);
		output->file_leaks = NULL;
	}
	if (output->file_bad_frees != NULL) {
		debugfs_remove(output->file_bad_frees);
		output->file_bad_frees = NULL;
	}
	if (output->file_stats != NULL) {
		debugfs_remove(output->file_stats);
		output->file_stats = NULL;
	}
	if (output->file_flush != NULL) {
		debugfs_remove(output->file_flush);
		output->file_flush = NULL;
	}
	if (output->file_clear != NULL) {
		debugfs_remove(output->file_clear);
		output->file_clear = NULL;
	}
}

/* [NB] We do not check here if debugfs is supported because this is done 
 * when creating the directory for these files ('dir_klc_main'). */
static int
klc_create_debugfs_files(struct kedr_lc_output *output, 
	struct kedr_leak_check *lc)
{
	BUG_ON(output == NULL);
	BUG_ON(dir_klc_main == NULL);
	
	output->file_leaks = debugfs_create_file("possible_leaks", 
		S_IRUGO, dir_klc_main, &output->ob_leaks, &klc_fops);
	if (output->file_leaks == NULL) 
		goto fail;
	
	output->file_bad_frees = debugfs_create_file("unallocated_frees", 
		S_IRUGO, dir_klc_main, &output->ob_bad_frees, &klc_fops);
	if (output->file_bad_frees == NULL) 
		goto fail;
	
	output->file_stats = debugfs_create_file("info", 
		S_IRUGO, dir_klc_main, &output->ob_other, &klc_fops);
	if (output->file_stats == NULL) 
		goto fail;

	output->file_flush = debugfs_create_file("flush",
		S_IWUSR | S_IWGRP, dir_klc_main, lc, &klc_flush_ops);
	if (output->file_flush == NULL)
		goto fail;

	output->file_clear = debugfs_create_file("clear",
		S_IWUSR | S_IWGRP, dir_klc_main, lc, &klc_clear_ops);
	if (output->file_clear == NULL)
		goto fail;

	return 0;

fail:
	pr_warn(KEDR_LC_MSG_PREFIX
		"failed to create output files in debugfs\n");
	klc_remove_debugfs_files(output);
	return -EINVAL;    
}
/* ====================================================================== */

int 
kedr_lc_output_init(void)
{
	/* Create the main directory for LeakCheck in debugfs */
	dir_klc_main = debugfs_create_dir("kedr_leak_check", NULL);
	if (IS_ERR(dir_klc_main)) {
		pr_warn(KEDR_LC_MSG_PREFIX
			"debugfs is not supported\n");
		dir_klc_main = NULL;
		return -ENODEV;
	}
	
	if (dir_klc_main == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX
			"failed to create a directory in debugfs\n");
		return -EINVAL;
	}
	return 0;
}

void
kedr_lc_output_fini(void)
{
	if (dir_klc_main != NULL) 
		debugfs_remove(dir_klc_main);
}
/* ====================================================================== */

struct kedr_lc_output *
kedr_lc_output_create(struct kedr_leak_check *lc)
{
	int ret = 0;
	struct kedr_lc_output *output = NULL;
	
	output = kzalloc(sizeof(*output), GFP_KERNEL);
	if (output == NULL) 
		return ERR_PTR(-ENOMEM);
	/* [NB] All fields of '*output' are now 0 or NULL. */
	
	ret = klc_output_buffer_init(&output->ob_leaks);
	if (ret != 0) 
		goto out_ob;
	ret = klc_output_buffer_init(&output->ob_bad_frees);
	if (ret != 0) 
		goto out_ob;
	ret = klc_output_buffer_init(&output->ob_other);
	if (ret != 0) 
		goto out_ob;

	ret = klc_create_debugfs_files(output, lc);
	if (ret != 0) 
		goto out_ob;
	
	return output;
out_ob:
	klc_output_buffer_cleanup(&output->ob_other);
	klc_output_buffer_cleanup(&output->ob_bad_frees);
	klc_output_buffer_cleanup(&output->ob_leaks);
	kfree(output);
	return ERR_PTR(ret);
}

void
kedr_lc_output_destroy(struct kedr_lc_output *output)
{
	if (output == NULL)
		return;
	
	klc_remove_debugfs_files(output);
	klc_output_buffer_cleanup(&output->ob_other);
	klc_output_buffer_cleanup(&output->ob_bad_frees);
	klc_output_buffer_cleanup(&output->ob_leaks);
	kfree(output);
}

void
kedr_lc_output_clear(struct kedr_lc_output *output)
{
	BUG_ON(output ==  NULL);
	
	/* "Clear" the data without actually releasing memory. 
	 * No need for locking as the caller ensures that no output may 
	 * interfere. */
	output->ob_leaks.data_len = 0;
	output->ob_leaks.buf[0] = '\0';
	
	output->ob_bad_frees.data_len = 0;
	output->ob_bad_frees.buf[0] = '\0';
	
	output->ob_other.data_len = 0;
	output->ob_other.buf[0] = '\0';
}
/* ====================================================================== */

void
klc_print_string(struct kedr_lc_output *output, 
	enum klc_output_type output_type, const char *s)
{
	struct klc_output_buffer *ob = NULL;
	
	BUG_ON(s == NULL);

	switch (output_type) {
	case KLC_UNFREED_ALLOC:
		ob = &output->ob_leaks;
		break;
	case KLC_BAD_FREE: 
		ob = &output->ob_bad_frees;
		break;
	case KLC_OTHER:
		ob = &output->ob_other;
		break;
	default:
		pr_warn(KEDR_LC_MSG_PREFIX 
			"unknown output type: %d\n", 
			(int)output_type);
		return;
	}
	BUG_ON(ob->buf == NULL); 
	
	if (mutex_lock_killable(&ob->lock) != 0)
	{
		pr_warn(KEDR_LC_MSG_PREFIX "klc_print_string(): "
			"got a signal while trying to acquire a mutex.\n");
		return;
	}
	
	klc_output_buffer_append(ob, s);
	klc_output_buffer_append(ob, "\n");
	
	if (syslog_output)
		pr_warn(KEDR_LC_MSG_PREFIX "%s\n", s);
	
	mutex_unlock(&ob->lock);
}

void
klc_print_stack_trace(struct kedr_lc_output *output, 
	enum klc_output_type output_type, 
	struct stack_entry **stack_entries, unsigned int num_entries)
{
	static const char* fmt = "[<%lx>] %s";
	char *buf = NULL;
	int len;
	unsigned int i;
	
	BUG_ON(stack_entries == NULL);
	
	if (num_entries == 0)
		return;
	
	kedr_lc_resolve_stack_entries(stack_entries, num_entries);

	for (i = 0; i < num_entries; ++i) {
		len = snprintf(NULL, 0, fmt, 
			(unsigned long)stack_entries[i]->addr, stack_entries[i]->symbolic);
		buf = kmalloc(len + 1, GFP_KERNEL);
		if (buf != NULL) {
			snprintf(buf, len + 1, fmt, 
				(unsigned long)stack_entries[i]->addr, stack_entries[i]->symbolic);
			klc_print_string(output, output_type, buf);
			kfree(buf);
		} else { 
			pr_warn(KEDR_LC_MSG_PREFIX
			"klc_print_stack_trace(): not enough memory "
			"to prepare a message of size %d\n",
				len);
		}
	}
}

void
kedr_lc_print_target_info(struct kedr_lc_output *output, 
	struct module *target, void *init_area, void *core_area)
{
	static const char* fmt = 
"Target module: \"%s\", init area at %lx, core area at %lx";
	char *buf = NULL;
	int len;
	const char *name;
	
	name = module_name(target);
	len = snprintf(NULL, 0, fmt, name, (unsigned long)init_area, (unsigned long)core_area);
	buf = kmalloc(len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX 
		"klc_print_target_info(): "
		"not enough memory to prepare a message of size %d\n",
			len);
		return;
	}
	snprintf(buf, len + 1, fmt, name, (unsigned long)init_area, (unsigned long)core_area);
	klc_print_string(output, KLC_OTHER, buf);
	kfree(buf);
}

/* A helper function to print an unsigned 64-bit value using the specified
 * format. The format must contain "%llu", "%llx" or the like. */
static void 
klc_print_u64(struct kedr_lc_output *output, 
	enum klc_output_type output_type, u64 data, const char *fmt)
{
	char *buf = NULL;
	int len;
	
	BUG_ON(fmt == NULL);
	
	len = snprintf(NULL, 0, fmt, data);
	buf = kmalloc(len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX "klc_print_u64(): "
		"not enough memory to prepare a message of size %d\n",
			len);
		return;
	}
	snprintf(buf, len + 1, fmt, data);
	klc_print_string(output, output_type, buf);
	kfree(buf);
}

static void
klc_print_process_info(struct kedr_lc_output *output,
	struct kedr_lc_resource_info *info,
	enum klc_output_type output_type)
{
	static const char* fmt_process_info =
		"Process: %s (PID: %d)";

	char *buf = NULL;
	int len;

	if (info->task_pid == -1) {
		klc_print_string(output, output_type, "<IRQ>");
		return;
	}
	
	/*
	 * TODO: info->task_comm may be not null-terminated array of bytes.
	 * "%s" specifier is not sufficient for print that array.
	 */
	len = snprintf(NULL, 0, fmt_process_info, info->task_comm,
		       (int)info->task_pid);
	buf = kmalloc(len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX "klc_print_process_info(): "
		"not enough memory to prepare a message of size %d\n",
			len);
		return;
	}
	snprintf(buf, len + 1, fmt_process_info, info->task_comm,
		(int)info->task_pid);
	klc_print_string(output, output_type, buf);
	kfree(buf);
}

void 
kedr_lc_print_alloc_info(struct kedr_lc_output *output, 
	struct kedr_lc_resource_info *info, u64 similar_allocs)
{
	static const char* fmt_common = 
	"Address: 0x%lx, size: %zu; stack trace of the allocation:";
	
	static const char* fmt_unknown = 
	"Address: 0x%lx, size: unknown; stack trace of the allocation:";
	
	char *buf = NULL;
	int len;
	
	BUG_ON(info == NULL);
	
	klc_print_process_info(output, info, KLC_UNFREED_ALLOC);
	
	if (info->size != 0) {
		len = snprintf(NULL, 0, fmt_common, (unsigned long)info->addr,
			info->size);
	}
	else {
		len = snprintf(NULL, 0, fmt_unknown, (unsigned long)info->addr);
	}
	buf = kmalloc(len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX "klc_print_alloc_info(): "
		"not enough memory to prepare a message of size %d\n",
			len);
		return;
	}
	
	if (info->size != 0) {
		snprintf(buf, len + 1, fmt_common, (unsigned long)info->addr, info->size);
	}
	else {
		snprintf(buf, len + 1, fmt_unknown, (unsigned long)info->addr);
	}
	klc_print_string(output, KLC_UNFREED_ALLOC, buf);
	kfree(buf);
	
	klc_print_stack_trace(output, KLC_UNFREED_ALLOC, 
		info->stack_entries, info->num_entries);
	
	if (similar_allocs != 0) {
		klc_print_u64(output, KLC_UNFREED_ALLOC, similar_allocs, 
		"+%llu more allocation(s) with the same call stack.");
	}
	
	klc_print_string(output, KLC_UNFREED_ALLOC, sep); /* separator */
}

void 
kedr_lc_print_dealloc_info(struct kedr_lc_output *output, 
	struct kedr_lc_resource_info *info, u64 similar_deallocs)
{
	static const char* fmt = 
		"Address: 0x%lx; stack trace of the deallocation:";
	char *buf = NULL;
	int len;
 
	BUG_ON(info == NULL);
	
	klc_print_process_info(output, info, KLC_BAD_FREE);
	
	len = snprintf(NULL, 0, fmt, (unsigned long)info->addr);
	buf = kmalloc(len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_warn(KEDR_LC_MSG_PREFIX 
		"klc_print_dealloc_info(): "
		"not enough memory to prepare a message of size %d\n",
			len);
		return;
	}
	snprintf(buf, len + 1, fmt, (unsigned long)info->addr);
	klc_print_string(output, KLC_BAD_FREE, buf);
	kfree(buf);
	
	klc_print_stack_trace(output, KLC_BAD_FREE, 
		info->stack_entries, info->num_entries);
	
	if (similar_deallocs != 0) {
		klc_print_u64(output, KLC_BAD_FREE, similar_deallocs, 
		"+%llu more deallocation(s) with the same call stack.");
	}
	
	klc_print_string(output, KLC_BAD_FREE, sep); /* separator */
}

void
kedr_lc_print_totals(struct kedr_lc_output *output, 
	u64 total_allocs, u64 total_leaks, u64 total_bad_frees)
{
	if (syslog_output != 0)
		pr_warn(KEDR_LC_MSG_PREFIX "Totals:\n");
	
	klc_print_u64(output, KLC_OTHER, total_allocs, 
		"Allocations: %llu");
	klc_print_u64(output, KLC_OTHER, total_leaks,
		"Possible leaks: %llu");
	klc_print_u64(output, KLC_OTHER, total_bad_frees,
		"Unallocated frees: %llu");
}

void 
kedr_lc_print_dealloc_note(struct kedr_lc_output *output, 
	u64 reported, u64 total)
{
	if (reported == total)
		return;
	
	klc_print_u64(output, KLC_BAD_FREE, reported,
"The information about only %llu of the \"unallocated free\" events is " 
"shown above. The data for other such events have been discarded to "
"save memory.");
}
/* ====================================================================== */

