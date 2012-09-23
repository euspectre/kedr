#include <trace_file.h>

#include <linux/module.h>
#include <linux/init.h>

#include <linux/mutex.h>

#include <linux/debugfs.h>

#include <linux/uaccess.h>

#include <linux/moduleparam.h>

#include <linux/slab.h> /*kmalloc and others*/

#include <linux/poll.h>

#include <linux/sched.h> /*task_tgid_vnr()*/

#define BUFFER_SIZE_DEFAULT 100000
#define TRACE_DIR_NAME "kedr_tracing"

//Initial buffer size
unsigned long buffer_size = BUFFER_SIZE_DEFAULT;
module_param(buffer_size, ulong, S_IRUGO);

// Names of files
static const char* trace_dir_name = TRACE_DIR_NAME;

static const char* reset_file_name = "reset";
static const char* buffer_size_file_name = "buffer_size";
static const char* lost_messages_file_name = "lost_messages";

//
static struct dentry* trace_dir;
static struct dentry* reset_file;
static struct dentry* buffer_size_file;
static struct dentry* lost_messages_file;

// Global trace_file object.
static struct trace_file* trace_file;
//Define types instead of including header
typedef int (*kedr_trace_pp_function)(char* dest, size_t size, const void* data);

/*
 * Format of the message written into the trace buffer.
 */
struct kedr_trace_message
{
    char command[TASK_COMM_LEN];
    pid_t pid;
    kedr_trace_pp_function pp;
    char data[0];
};

///////Implementation of the exported functions/////////////////

/*
 * Unregister pretty print for data.
 * Clean all data from the trace.
 */
void kedr_trace_pp_unregister(void)
{
    trace_file_reset(trace_file);
}
EXPORT_SYMBOL(kedr_trace_pp_unregister);

/*
 * Reserve space for message in the trace.
 * 
 * 'pp' is pretty print function which will be used for this data.
 * 
 * Return not NULL on success. Returning value should be passed to
 * the kedr_trace_unlock_commit() for complete trace operation.
 */
void* kedr_trace_lock(kedr_trace_pp_function pp,
    size_t size, void** data)
{
    struct kedr_trace_message* msg;
    void* id = trace_file_write_lock(trace_file,
        sizeof(struct kedr_trace_message) + size, (void**)&msg);
    if(id == NULL) return NULL;
    msg->pp = pp;
    msg->pid = task_tgid_vnr(current);
    strncpy(msg->command, current->comm, sizeof(msg->command));

    *data = msg->data;
    return id;
}
EXPORT_SYMBOL(kedr_trace_lock);

/*
 * Complete trace operation, started with kedr_trace_lock().
 */

void kedr_trace_unlock_commit(void* id)
{
    trace_file_write_unlock(trace_file, id);
}
EXPORT_SYMBOL(kedr_trace_unlock_commit);

/*
 * Add message into the trace.
 * 
 * 'pp' is pretty print function which will be used for this data.
 */
void kedr_trace(kedr_trace_pp_function pp,
    const void* data, size_t size)
{
    void* vdata;
    void* id = kedr_trace_lock(pp, size, &vdata);
    if(id)
    {
        memcpy(vdata, data, size);
        kedr_trace_unlock_commit(id);
    }
}
EXPORT_SYMBOL(kedr_trace);



/*
 * Interpretator of the trace content.
 * 
 * In the current implementation it interpret message as string
 * (see comments to the rb_test_trace_write()) and write as text
 * with cpu number and timestamp.
 * 
 * Real application may interpret the message as some struct,
 * and choose format according to some field(s) of this struct.
 */

int trace_print_message(char* str, size_t size,
    const void* msg, size_t msg_size, int cpu, u64 ts, void* user_data);

// Reset buffer file operations
// Do nothing
static ssize_t reset_file_write(struct file *filp,
    const char __user *buf, size_t count, loff_t * f_pos);
// Reset trace.
static int reset_file_open(struct inode *inode, struct file *filp);

static struct file_operations reset_file_ops = 
{
    .owner = THIS_MODULE,
    .open = reset_file_open,
    .write = reset_file_write,
};

// Buffer size file operations
static int
buffer_size_file_open(struct inode *inode, struct file *filp);
static int
buffer_size_file_release(struct inode *inode, struct file *filp);
// Set size of the trace buffer
static ssize_t
buffer_size_file_write(struct file *filp,
    const char __user *buf, size_t count, loff_t * f_pos);
// Get size of the trace buffer
static ssize_t
buffer_size_file_read(struct file *filp,
    char __user* buf, size_t count, loff_t *f_pos);

static struct file_operations buffer_size_file_ops = 
{
    .owner = THIS_MODULE,
    .open = buffer_size_file_open,
    .release = buffer_size_file_release,
    .read = buffer_size_file_read,
    .write = buffer_size_file_write,
};

// Lost messages file operations
static int
lost_messages_file_open(struct inode *inode, struct file *filp);
static int
lost_messages_file_release(struct inode *inode, struct file *filp);
// Return number of messages which lost.
static ssize_t
lost_messages_file_read(struct file *filp,
    char __user* buf, size_t count, loff_t *f_pos);

static struct file_operations lost_messages_file_ops = 
{
    .owner = THIS_MODULE,
    .open = lost_messages_file_open,
    .release = lost_messages_file_release,
    .read = lost_messages_file_read,
};

static int __init
rb_test_init(void)
{
    trace_dir = debugfs_create_dir(trace_dir_name, NULL);
    if(trace_dir == NULL)
    {
        pr_err("Cannot create trace directory in debugfs.");
        return -EINVAL;
    }

    trace_file = trace_file_create(buffer_size, 1,
        trace_dir, THIS_MODULE,
        trace_print_message, NULL);
    
    if(trace_file == NULL)
    {
        debugfs_remove(trace_dir);
        return -EINVAL;
    }

    reset_file = debugfs_create_file(reset_file_name,
        S_IWUSR | S_IWGRP,
        trace_dir,
        trace_file,
        &reset_file_ops);
    if(reset_file == NULL)
    {
        pr_err("Cannot create reset file.");
        trace_file_destroy(trace_file);
        debugfs_remove(trace_dir);
        return -EINVAL;
    }

    buffer_size_file = debugfs_create_file(buffer_size_file_name,
        S_IRUGO | S_IWUSR | S_IWGRP,
        trace_dir,
        trace_file,
        &buffer_size_file_ops);
    if(buffer_size_file == NULL)
    {
        pr_err("Cannot create file for control size of buffer.");
        debugfs_remove(reset_file);
        trace_file_destroy(trace_file);
        debugfs_remove(trace_dir);
        return -EINVAL;
    }
    
    lost_messages_file = debugfs_create_file(lost_messages_file_name,
        S_IRUGO,
        trace_dir,
        trace_file,
        &lost_messages_file_ops);
    if(lost_messages_file == NULL)
    {
        pr_err("Cannot create file for control number of messages lost.");
        debugfs_remove(buffer_size_file);
        debugfs_remove(reset_file);
        trace_file_destroy(trace_file);
        debugfs_remove(trace_dir);
        return -EINVAL;
    }

    return 0;
}

static void __exit
rb_test_exit(void)
{
    debugfs_remove(lost_messages_file);
    debugfs_remove(buffer_size_file);
    debugfs_remove(reset_file);
    trace_file_destroy(trace_file);
    debugfs_remove(trace_dir);
}

module_init(rb_test_init);
module_exit(rb_test_exit);

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_DESCRIPTION("KEDR tracing system");
MODULE_LICENSE("GPL");
////////////////////////////////////

// Reset buffer file operations implementation
static ssize_t
reset_file_write(struct file *filp,
    const char __user *buf, size_t count, loff_t * f_pos)
{
    // Do nothing, trace reseting already performed while opening file
    return count;
}

static int
reset_file_open(struct inode *inode, struct file *filp)
{
    if((filp->f_flags & O_ACCMODE) != O_RDONLY)
    {
        struct trace_file* trace_file =
            (struct trace_file*)inode->i_private;
        trace_file_reset(trace_file);
    }
    return nonseekable_open(inode, filp);
}

// Buffer size file operations implementation
static int
buffer_size_file_open(struct inode *inode, struct file *filp)
{
    int result;
    if((filp->f_flags & O_ACCMODE) != O_WRONLY)
    {
        struct trace_file* trace_file =
            (struct trace_file*)inode->i_private;
        
        unsigned long size = trace_file_size(trace_file);
        size_t size_len;
        char* size_str;
        size_len = snprintf(NULL, 0, "%lu\n", size);
        size_str = kmalloc(size_len + 1, GFP_KERNEL);
        if(size_str == NULL)
        {
            pr_err("buffer_size_file_open: Cannot allocate string.");
            return -ENOMEM;
        }
        snprintf(size_str, size_len + 1, "%lu\n", size);
        filp->private_data = size_str;
    }
    result = nonseekable_open(inode, filp);
    if(result)
    {
        kfree(filp->private_data);
    }
    return result;
}
static int
buffer_size_file_release(struct inode *inode, struct file *filp)
{
    kfree(filp->private_data);
    return 0;
}

static ssize_t
buffer_size_file_write(struct file *filp,
    const char __user *buf, size_t count, loff_t * f_pos)
{
    int error = 0;
    unsigned long size;
    
    struct trace_file* trace_file =
        (struct trace_file*)filp->f_dentry->d_inode->i_private;

    
    if(count == 0) return -EINVAL;
    {
        char* str = kmalloc(count + 1, GFP_KERNEL);
        if(str == NULL)
        {
            return -ENOMEM;
        }
        if(copy_from_user(str, buf, count))
        {
            kfree(str);
            return -EFAULT;
        }
        str[count] = '\0';
        error = strict_strtoul(str, 0, &size);
        kfree(str);
        if(error) return error;
    }
    error = trace_file_size_set(trace_file, size);
    return error ? error : count;
}
static ssize_t
buffer_size_file_read(struct file *filp,
    char __user* buf, size_t count, loff_t *f_pos)
{
    const char* size_str = filp->private_data;
    size_t size_len = strlen(size_str);
    
    return simple_read_from_buffer(buf, count, f_pos, size_str, size_len);
}

// Lost messages file operations implementation
static int
lost_messages_file_open(struct inode *inode, struct file *filp)
{
    int result;
    size_t len;
    char* str;

    struct trace_file* trace_file =
        (struct trace_file*)inode->i_private;

    unsigned long lost_messages =
        trace_file_lost_messages(trace_file);

    len = snprintf(NULL, 0, "%lu\n", lost_messages);
    str = kmalloc(len + 1, GFP_KERNEL);
    if(str == NULL)
    {
        pr_err("lost_messages_file_open: Cannot allocate string.");
        return -ENOMEM;
    }
    snprintf(str, len + 1, "%lu\n", lost_messages);
    filp->private_data = str;
    result = nonseekable_open(inode, filp);
    if(result)
    {
        kfree(str);
    }
    return result;
}

static int
lost_messages_file_release(struct inode *inode, struct file *filp)
{
    char* str = filp->private_data;
    kfree(str);
    return 0;
}

static ssize_t
lost_messages_file_read(struct file *filp,
    char __user* buf, size_t count, loff_t *f_pos)
{
    const char* str = filp->private_data;
    size_t len = strlen(str);
    
    return simple_read_from_buffer(buf, count, f_pos, str, len);
}

int trace_print_message(char* str, size_t size,
    const void* msg, size_t msg_size, int cpu, u64 ts, void* user_data)
{
    int result, result_total = 0;
    const struct kedr_trace_message* msg_real =
        (const struct kedr_trace_message*)msg;
    // ts is time in nanoseconds since system starts
    u32 sec, ms;
   
    sec = div_u64_rem(ts, 1000000000, &ms);
    ms /= 1000;

    (void)user_data;

#define update_str(str, size, result) \
    if(size <= result) { str += size; size = 0; } \
    else{ str += result; size -= result; }

    result = snprintf(str, size, "%s-%d\t[%.03d]\t%lu.%.06u:\t",
        msg_real->command, msg_real->pid,
        cpu, (unsigned long)sec, (unsigned)ms);
    result_total += result;

    update_str(str, size, result);
    result = msg_real->pp(str,
        size, msg_real->data);
    result_total += result;

    update_str(str, size, result);
    result = snprintf(str, size, "\n");
    result_total += result;
#undef update_str
    return result_total;
}

/*
 * Auxiliary functions for trace target session markers
 * (use only for KEDR).
 * 
 * Implementation.
 */

static int
kedr_trace_marker_target_pp_function(char* dest, size_t size,
    const void* data)
{
    bool is_begin;
    const char* target_name, *payload_name;

    is_begin = *(bool*)data;
    target_name = (const char*)data + sizeof(bool);
    payload_name = target_name + strlen(target_name) + 1;
    
    return snprintf(dest, size,
        "target_session_%s: target_name: \"%s\", payload_name: \"%s\"",
            is_begin? "begins" : "ends",
            target_name,
            payload_name);
}

void kedr_trace_marker_target(struct module* target_module,
    struct module* payload_module, bool is_begin)
{
    const char* target_name, *payload_name;
    void* id;
    size_t size;
    void* data;
    
    target_name = module_name(target_module);
    payload_name = module_name(payload_module);
    
    size = sizeof(bool) + strlen(target_name) + strlen(payload_name) + 2;
    
    id = kedr_trace_lock(kedr_trace_marker_target_pp_function,
        size, &data);
    
    if(id == NULL) return;
    
    *(bool*)data = is_begin;
    strcpy(data + sizeof(bool), target_name);
    strcpy(data + sizeof(bool) + strlen(target_name) + 1, payload_name);
    
    kedr_trace_unlock_commit(id);
}
EXPORT_SYMBOL(kedr_trace_marker_target);
