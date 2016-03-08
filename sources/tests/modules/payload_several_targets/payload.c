
/*********************************************************************
 * Similar to ../payload/payload.c, but support several targets.
 * 
 * Maintain list of loaded targets and count of 'kfree' calls in
 * target modules. This info is exported via debugfs.
 *********************************************************************/

/* ========================================================================
 * Copyright (C) 2012-2014, KEDR development team
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
#include <linux/init.h>
#include <linux/slab.h> /* Intercept kfree. */
#include <linux/debugfs.h> /* files for read info collected. */
#include <linux/seq_file.h> /* read from debugfs files. */
#include <linux/list.h> /* List of targets is organize via struct list_head. */

#include <kedr/core/kedr.h>

/*********************************************************************/
MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");
/*********************************************************************/

atomic_t kfree_counter = ATOMIC_INIT(0);
struct dentry *kfree_counter_file = NULL;


int is_session_started = 0;
LIST_HEAD(targets_list);

/* One element of targets_list. */
struct target_elem
{
    struct module *m;
    struct list_head list;
};
DEFINE_MUTEX(targets_mutex);
struct dentry *targets_list_file = NULL;


char *error_str = NULL;
DEFINE_MUTEX(error_mutex);
struct dentry *error_file = NULL;

/***************** Error file implementation **************************/
// Set error string if it has not set before.
void set_error(const char* str)
{
    mutex_lock(&error_mutex);
    if(!error_str)
    {
        error_str = kstrdup(str, GFP_KERNEL);
    }
    mutex_unlock(&error_mutex);
}

int show_error(struct seq_file* m, void* data)
{
    (void)data;
    mutex_lock(&error_mutex);
    if(error_str)
    {
        seq_write(m, error_str, strlen(error_str));
        seq_putc(m, '\n');
    }
    mutex_unlock(&error_mutex);
    
    return 0;
}

int error_file_open(struct inode* inode, struct file* filp)
{
    return single_open(filp, &show_error, NULL);
}

static const struct file_operations error_file_ops =
{
    .owner = THIS_MODULE,
    
    .open = error_file_open,
    .read = seq_read,
    .release = single_release
};

/***************** Kfree counter file implementation ******************/
void kfree_pre_handler(void *p, struct kedr_function_call_info *call_info)
{
    (void)p;
    (void)call_info;
    
    atomic_inc(&kfree_counter);
}

int show_kfree_counter(struct seq_file *m, void *data)
{
    (void)data;
    
    seq_printf(m, "%d\n", (int)atomic_read(&kfree_counter));
    
    return 0;
}

int kfree_file_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &show_kfree_counter, NULL);
}

static struct file_operations kfree_file_ops =
{
    .owner = THIS_MODULE,
    
    .open = kfree_file_open,
    .read = seq_read,
    .release = single_release
};

/***************** Targets list file implementation ******************/
void on_session_start(void)
{
    if(is_session_started) set_error("Attempt to start session which already started");
    else is_session_started = 1;
}

void on_session_end(void)
{
    if(!is_session_started) set_error("Attempt to stop session which has not started");
    else is_session_started = 0;
}

void on_target_loaded(struct module *m)
{
    struct target_elem *elem;

    if(!is_session_started)
        set_error("on_target_loaded is called without session started");
    
    elem = kmalloc(sizeof(*elem), GFP_KERNEL);
    if(elem)
    {
        elem->m = m;

        mutex_lock(&targets_mutex);
        list_add_tail(&elem->list, &targets_list);
        mutex_unlock(&targets_mutex);
    }
}

void on_target_about_to_unload(struct module *m)
{
    struct target_elem *elem;

    if(!is_session_started)
        set_error("on_target_about_to_unload is called without session started");
    
    mutex_lock(&targets_mutex);
    list_for_each_entry(elem, &targets_list, list)
    {
        if(elem->m == m)
            break;
    }
    if(&elem->list != &targets_list)
    {
        list_del(&elem->list);
        kfree(elem);
    }
    else
    {
        set_error("Cannot find target for remove");
    }
    mutex_unlock(&targets_mutex);
}

void* targets_list_seq_start(struct seq_file *m, loff_t *pos)
{
    mutex_lock(&targets_mutex);
    return seq_list_start(&targets_list, *pos);
}

void targets_list_seq_stop(struct seq_file *m, void *v)
{
    (void)v;
    mutex_unlock(&targets_mutex);
}

void* targets_list_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    return seq_list_next(v, &targets_list, pos);
}

int targets_list_seq_show(struct seq_file* m, void* v)
{
    const char* name;
    struct target_elem* elem = list_entry(v, typeof(*elem), list);
    
    name = module_name(elem->m);
    seq_write(m, name, strnlen(name, MODULE_NAME_LEN));
    if(elem->list.next != &targets_list)
    {
        // Append comma after each module name except the last one.
        seq_putc(m, ',');
    }
    else if(!list_empty(&targets_list))
    {
        // Add newline symbol after non-empty output.
        seq_putc(m, '\n');
    }
    
    return 0;
}

static struct seq_operations targets_list_seq_ops =
{
    .start = targets_list_seq_start,
    .stop = targets_list_seq_stop,
    .next = targets_list_seq_next,
    .show = targets_list_seq_show
};

int targets_list_file_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &targets_list_seq_ops);
}

static struct file_operations targets_list_file_ops =
{
    .owner = THIS_MODULE,
    
    .open = targets_list_file_open,
    .read = seq_read,
    .release = seq_release
};

/**********************************************************************/
struct kedr_pre_pair pre_pairs[] =
{
    {
        .orig = kfree,
        .pre = kfree_pre_handler
    },
    {
        .orig = NULL
    }
};

struct kedr_payload payload = 
{
    .mod = THIS_MODULE,
    
    .pre_pairs = pre_pairs,
    
    .on_session_start = on_session_start,
    .on_session_end = on_session_end,
    
    .on_target_loaded = on_target_loaded,
    .on_target_about_to_unload = on_target_about_to_unload
};

/* Shorcat for create file and detect error. */
static int create_info_file(struct dentry **dentry, const char* name,
    const struct file_operations* ops)
{
    *dentry = debugfs_create_file(name, S_IRUGO,
        NULL, NULL, ops);
    
    if(!*dentry)
        return -EINVAL;
    else if (IS_ERR(*dentry))
        return PTR_ERR(*dentry);
    else
        return 0;
}

extern int functions_support_register(void);
extern void functions_support_unregister(void);


static int __init payload_init(void)
{
    int result;
    
    result = functions_support_register();
    if(result) return result;
    
    result = kedr_payload_register(&payload);
    if(result) goto err_payload;
    
    result = create_info_file(&kfree_counter_file,
        "kedr_test_kfree_counter", &kfree_file_ops);
    if(result) goto err_kfree_counter_file;
    
    result = create_info_file(&targets_list_file,
        "kedr_test_targets_list", &targets_list_file_ops);
    if(result) goto err_targets_list_file;
    
    result = create_info_file(&error_file,
        "kedr_test_error", &error_file_ops);
    if(result) goto err_error_file;

    return 0;


err_error_file:
    debugfs_remove(targets_list_file);
err_targets_list_file:
    debugfs_remove(kfree_counter_file);
err_kfree_counter_file:
    kedr_payload_unregister(&payload);
err_payload:
    functions_support_unregister();
    
    return result;
}

static void __exit payload_exit(void)
{
    debugfs_remove(error_file);
    debugfs_remove(targets_list_file);
    debugfs_remove(kfree_counter_file);

    kedr_payload_unregister(&payload);
    functions_support_unregister();
}

module_init(payload_init);
module_exit(payload_exit);