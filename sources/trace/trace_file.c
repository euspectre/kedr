#include <trace_file.h>

#include <linux/module.h>

#include <linux/mutex.h>

#include <linux/debugfs.h>

#include <linux/uaccess.h>

#include <linux/slab.h> /*kmalloc and others*/

#include <linux/poll.h>

// Name of trace file
static const char* trace_file_name = "trace";

/*
 * Struct, which implements trace_file.
 */
struct trace_file
{
    //buffer with 'archived' messages
    struct trace_buffer* trace_buffer;
    //last message in 'plain' form
    char* start;//allocated memory
    char* end;//pointer after the end of the buffer
    char* current_pos;//pointer to the first unread symbol
    //protect the message in 'plain' form from concurrent access.
    struct mutex m;
    //Trace file
    struct dentry* file;
    //Copy of file operations with module set.
    struct file_operations trace_file_ops;
    // Trace buffer interpretator
    snprintf_message print_message;
    void* user_data;
};


/*
 * Updater for last message in plain form.
 * 
 * If plain form of last message is empty, it should set it
 * according to the last message in the trace buffer.
 * 
 * Return 1 if plain message become non-empty, negative error code otherwise.
 * 
 * NOTE: Do not return 0 for do not confuse with buffer emptiness
 * in non-block reading(see trace_buffer_read_message()).
 */
static int trace_process_data(const void* msg,
    size_t size, int cpu, u64 ts, bool* consume, void* user_data);


// Trace file operations
static int trace_file_open(struct inode *inode, struct file *filp);
// Consume messages from the trace.
static ssize_t trace_file_read(struct file *filp,
    char __user* buf, size_t count, loff_t *f_pos);
// Wait until message will be available in the trace_file.
static unsigned int trace_file_poll(struct file *filp, poll_table *wait);

static struct file_operations trace_file_ops = 
{
    .owner = NULL, //placeholder for module
    .open = trace_file_open,
    .read = trace_file_read,
    .poll = trace_file_poll,
};


//Implementation of trace file operations
static int trace_file_open(struct inode *inode, struct file *filp)
{
    filp->private_data = inode->i_private;
    return nonseekable_open(inode, filp);
}


ssize_t trace_file_read(struct file *filp,
    char __user* buf, size_t count, loff_t *f_pos)
{
    struct trace_file* trace_file = (struct trace_file*)filp->private_data;
    
    if(mutex_lock_interruptible(&trace_file->m))
        return -ERESTARTSYS;
    
    while(trace_file->end == trace_file->current_pos)
    {
        ssize_t error;
        mutex_unlock(&trace_file->m);
        error = trace_buffer_read_message(trace_file->trace_buffer,
            trace_process_data,
            !(filp->f_flags & O_NONBLOCK),
            trace_file);
        if(error < 0)
            return error;
        if(error == 0)
            return -EAGAIN;
        if(mutex_lock_interruptible(&trace_file->m))
            return -ERESTARTSYS;
        //need to verify that plain message is not empty again,
        //because someone may read it while we reaquire lock
    }
    if(count > (trace_file->end - trace_file->current_pos))
        count = trace_file->end - trace_file->current_pos;
    
    if(copy_to_user(buf, trace_file->current_pos, count) != 0)
    {
        mutex_unlock(&trace_file->m);
        return -EFAULT;
    }
    trace_file->current_pos += count;
    mutex_unlock(&trace_file->m);
    return count;
}

/*
 * Auxiliary struct for implement file's polling method via trace_buffer_poll_read.
 */
struct trace_file_poll_table
{
    struct file *filp;
    poll_table *wait;
};

static void trace_file_wait_function(wait_queue_head_t *q, void* data)
{
    struct trace_file_poll_table* table = (struct trace_file_poll_table*)data;
    poll_wait(table->filp, q, table->wait);
}

static unsigned int trace_file_poll(struct file *filp, poll_table *wait)
{
    int can_read = 0;//1 - can read, 0 - cannot read, <0 - error
    struct trace_file* trace_file = filp->private_data;
    
    // Fast path, without lock(!)
    can_read = (trace_file->current_pos != trace_file->end) ? 1 : 0;

    if(!can_read)
    {
        struct trace_file_poll_table table;
        table.filp = filp;
        table.wait = wait;
        can_read = trace_buffer_poll_read(trace_file->trace_buffer, trace_file_wait_function,
            &table);
    }
    
    return (can_read < 0) ? POLLERR : (can_read ? (POLLIN | POLLRDNORM) : 0);
}

//****************Implementation of the interface***************
/*
 * Write message to the trace.
 * 
 * Message is an array of bytes 'msg' with size 'msg_size'.
 */

void trace_file_write_message(struct trace_file* trace_file,
    const void* msg, size_t msg_size)
{
    trace_buffer_write_message(trace_file->trace_buffer, msg, msg_size);
}

/*
 * Reserve space in the buffer for writting message.
 * 
 * After call, pointer to the reserved space is saved in the 'msg'.
 * 
 * Return not-NULL identificator, which should be passed to
 * the trace_file_write_unlock() for commit writing.
 *
 * On error NULL is returned and 'msg' pointer shouldn't be used.
 * 
 * May be called in the atomic context.
 */
void* trace_file_write_lock(struct trace_file* trace_file,
    size_t size, void** msg)
{
    return trace_buffer_write_lock(trace_file->trace_buffer, size, msg);
}

/*
 * Commit message written after previous call
 * trace_buffer_write_lock().
 * 
 * May be called in the atomic context.
 */
void trace_file_write_unlock(struct trace_file* trace_file,
    void* id)
{
    trace_buffer_write_unlock(trace_file->trace_buffer, id);
}

/*
 * Create trace buffer with given buffer size and owerwrite mode.
 *
 * Create file in the given directory in debugfs, using which one can
 * read the trace.
 * Module 'm' is prevented from unload while trace file is opened.
 * 
 * 'print_message' is interpretator of trace buffer, 'user_data'
 * is the last parameter of this interpretator.
 */

struct trace_file* trace_file_create(
    size_t buffer_size, bool mode_overwrite,
    struct dentry* work_dir, struct module* m,
    snprintf_message print_message, void* user_data)
{
    struct trace_file* trace_file = kzalloc(sizeof(*trace_file), GFP_KERNEL);
    if(trace_file == NULL)
    {
        pr_err("Cannot allocate 'trace_file' structure.");
        return NULL;
    }
    mutex_init(&trace_file->m);
    
    trace_file->trace_buffer = trace_buffer_alloc(buffer_size, 1);
    if(trace_file->trace_buffer == NULL)
    {
        kfree(trace_file);
        return NULL;
    }
    //
    mutex_init(&trace_file->m);
    trace_file->print_message = print_message;
    trace_file->user_data = user_data;

    // Create trace file
    memcpy(&trace_file->trace_file_ops, &trace_file_ops,
        sizeof(trace_file_ops));
    trace_file->trace_file_ops.owner = m;

    trace_file->file = debugfs_create_file(trace_file_name,
        S_IRUGO,
        work_dir,
        trace_file,
        &trace_file->trace_file_ops);
    
    if(trace_file->file == NULL)
    {
        pr_err("Cannot create trace file.");
        trace_buffer_destroy(trace_file->trace_buffer);
        kfree(trace_file);
        return NULL;
    }
    
    return trace_file;
}

/*
 * Remove trace buffer and trace file, which represent content of this
 * buffer.
 */
void trace_file_destroy(struct trace_file* trace_file)
{
    debugfs_remove(trace_file->file);
    mutex_destroy(&trace_file->m);
    trace_buffer_destroy(trace_file->trace_buffer);
    kfree(trace_file->start);
    kfree(trace_file);
}

/*
 * Reseting content of the trace file.
 */

void trace_file_reset(struct trace_file* trace_file)
{
    mutex_lock(&trace_file->m);
    kfree(trace_file->start);
    trace_file->start = NULL;
    trace_file->end = NULL;
    trace_file->current_pos = NULL;
    
    trace_buffer_reset(trace_file->trace_buffer);
    mutex_unlock(&trace_file->m);
}

/*
 * Return current size of trace buffer.
 * 
 * Note: This is not a size of the trace file.
 */

unsigned long trace_file_size(struct trace_file* trace_file)
{
    return trace_buffer_size(trace_file->trace_buffer);
}

/*
 * Reset content of the trace file and set new size for its buffer.
 * Return 0 on success, negative error code otherwise.
 */

int trace_file_size_set(struct trace_file* trace_file, unsigned long size)
{
    int error;
    if(mutex_lock_interruptible(&trace_file->m))
    {
        return -ERESTARTSYS;
    }
    kfree(trace_file->start);
    trace_file->start = NULL;
    trace_file->end = NULL;
    trace_file->current_pos = NULL;
    
    error = trace_buffer_resize(trace_file->trace_buffer, size);
    
    mutex_unlock(&trace_file->m);

    return error < 0 ? error : 0;
}

/*
 * Return number of messages lost since the trace file 
 * creation/last reseting.
 */

unsigned long trace_file_lost_messages(struct trace_file* trace_file)
{
    return trace_buffer_lost_messages(trace_file->trace_buffer);
}
    
//////////////
static int trace_process_data(const void* msg,
    size_t msg_size, int cpu, u64 ts, bool *consume, void* user_data)
{
    struct trace_file* trace_file = (struct trace_file*)user_data;
    
    //snprintf-like function, which print message into string,
    //which then will be read from the trace_file.
#define print_msg(buffer, size) trace_file->print_message(buffer, size, \
            msg, msg_size, cpu, ts, trace_file->user_data)

    size_t read_size;
   

    if(mutex_lock_interruptible(&trace_file->m))
    {
        return -ERESTARTSYS;
    }
    if(trace_file->current_pos != trace_file->end)
    {
        /*
         * Someone already update plain message, while we reaquiring lock.
         * So, silently ignore updating.
         */
        mutex_unlock(&trace_file->m);
        return 1;
    }
    
    read_size = print_msg(NULL, 0);//determine size of the message
    //Need to allocate buffer for message + '\0' byte, because
    //snprintf appends '\0' in any case, even if it does not need.
    trace_file->start = krealloc(trace_file->start, read_size + 1,
        GFP_KERNEL);
    if(trace_file->start)
    {
        // Real printing
        // read_size + 1 means size of message + '\0' byte
        print_msg(trace_file->start, read_size + 1);
        // We don't want to read '\0' byte, so silently ignore it
        // (read_size without "+1")
        trace_file->end = trace_file->start + read_size;
        trace_file->current_pos = trace_file->start;
        mutex_unlock(&trace_file->m);
        *consume = 1;//message is processed
        return 1;
    }
    mutex_unlock(&trace_file->m);
    return -ENOMEM;
#undef print_msg
}