#include <kedr/core/kedr.h>
#include "trace_buffer.h"
#include "wait_nestable.h"

#include <linux/module.h>
#include <linux/init.h>

#include <linux/mutex.h>

#include <linux/debugfs.h>

#include <linux/uaccess.h>

#include <linux/moduleparam.h>

#include <linux/slab.h> /*kmalloc and others*/

#include <linux/poll.h>

#include <linux/sched.h> /*task_tgid_vnr()*/

#include <linux/version.h> /* KERNEL_VERSION macro */

#include "config.h"

#define BUFFER_SIZE_DEFAULT 100000

// Global trace_buffer object.
static struct trace_buffer* tb_global;


/* 
 * Trace session descriptor.
 * 
 * Trace session is assigned for every message in the trace according to
 * its timestamp.
 * 
 * When read using 'trace_session' file, trace session of the first
 * message read is assigned to the file opened(via its ->private_data field).
 * 
 * After that, messages are returned to the file's reader while they
 * have the same trace session descriptor as one assigned to the file.
 * 
 * When file's trace session is ended, reading from the file return 0
 * (EOF mark) instead of -EAGAIN if trace is found to be empty.
 */
struct trace_session
{
    int refs;
    /* Element of 'trace_session_list', see below. */
    struct list_head list;
    /* Whether session is marked as ended.*/
    bool is_ended;
    /* End timestamp of the session (if 'is_ended' is true). */
    u64 ts_end;

    struct kedr_trace_callback_head callback_head;
};


/* 
 * List of 'active' sessions, which may correspond to messages in the
 * trace buffer.
 * 
 * All sessions except the last one are ended and their timestamps are
 * ordered in ascended order.
 * 
 * Last session is always non-ended, and it is interpreted as a session
 * for new messages written.
 */
static LIST_HEAD(trace_session_list);

/* Trace message as a plain text. */
struct trace_message_extracted
{
    /* 
     * Pointer to the allocated buffer with text.
     * 
     * If not allocated(and message is not stored), this is NULL.
     * 
     * NOTE: This may be not NULL even when message is not stored.
     */
    char* text;
    /*
     * Length(>=1) of the text.
     * 
     * In case no message is stored this field is 0.
     */
    size_t text_size;
    /* Allocated size of the text buffer. Useful for buffer's reusing. */
    size_t alloc_size;
    
    /* 
     * Referenced session descriptor, corresponded to given message.
     * 
     * The thing is that 'trace_session_list' contains only sessions
     * for messages in the trace buffer. But given message is stored
     * only as a plain text, not in the trace buffer.
     * 
     * If no message is stored, this field is NULL.
     */
    struct trace_session* message_session;
};

/* Last extracted message. */
struct trace_message_extracted tme_last;
/* 
 * Index of the the first unread symbol in the last message.
 * 
 * After whole message is read, message is immediately cleared and
 * this index become 0.
 */
static size_t tme_last_current_pos = 0;
/* Waitqueue for wait current session ends.*/
static DECLARE_WAIT_QUEUE_HEAD(wq_session);
/* 
 * Protect from concurrent access:
 * 
 * 'tme_last';
 * 'tme_last_current_pos';
 * 'trace_session_list';
 * methods on 'tb_global'.
 */
static DEFINE_MUTEX(trace_m);

/**************** Implementation of the trace session *****************/
static struct trace_session* trace_session_ref(struct trace_session* session)
{
    session->refs++;
    return session;
}

static void trace_session_unref(struct trace_session* session)
{
    if(--session->refs == 0)
    {
        kfree(session);
    }
}

static struct trace_session* trace_session_create(void)
{
    struct trace_session* session = kmalloc(sizeof(*session), GFP_KERNEL);
    
    if(session)
    {
        session->refs = 1;
        session->is_ended = 0;
    }
    
    return session;
}

static void trace_session_set_end(struct trace_session* session)
{
    session->ts_end = trace_buffer_clock(tb_global);
    session->is_ended = 1;
}

void trace_session_after_read_callback(struct kedr_trace_callback_head* callback_head)
{
    struct trace_session* session = container_of(callback_head, typeof(*session), callback_head);
    list_del(&session->list);
    trace_session_unref(session);
}

/* Mark current trace session as ended. */
static int kedr_trace_end_session(void)
{
    int err;
    struct trace_session* session, *session_new;
    
    /* 
     * Allocate structure for new session first.
     * 
     * If failed, whole operation is cancelled.
     */
    session_new = trace_session_create();
    if(!session_new) return -ENOMEM;
    
    err = mutex_lock_interruptible(&trace_m);
    if(err)
    {
        trace_session_unref(session_new);
        return err;
    }
    
    BUG_ON(list_empty(&trace_session_list));
    
    session = list_entry(trace_session_list.prev, typeof(*session), list);
    list_add_tail(&session_new->list, &trace_session_list);
    
    trace_session_set_end(session);
    /* 
     * This should be called under mutex locked, as callback may be
     * executed immediately.
     */
    kedr_trace_call_after_read(&trace_session_after_read_callback,
        &session->callback_head);

    mutex_unlock(&trace_m);
    
    wake_up_all(&wq_session);
    
    return 0;
}

/*********** Implementation of the extracted trace message ************/
static void tme_init(struct trace_message_extracted* tme)
{
    tme->text = NULL;
    tme->text_size = 0;
    tme->alloc_size = 0;
    
    tme->message_session = NULL;
}

static void tme_destroy(struct trace_message_extracted* tme)
{
    if(tme->text)
    {
        kfree(tme->text);
        if(tme->message_session)
            trace_session_unref(tme->message_session);
    }
}

/* Clear message stored. */
static void tme_clear(struct trace_message_extracted* tme)
{
    if(tme->text_size)
    {
        tme->text_size = 0;
        BUG_ON(!tme->message_session);
        trace_session_unref(tme->message_session);
        tme->message_session = NULL;
    }
}

/* Allocate text buffer at least of size 'size'. */
static int tme_alloc(struct trace_message_extracted* tme, size_t size)
{
    BUG_ON(tme->text_size);
    
    if(tme->alloc_size >= size) return 0; // Nothing to do
    
    kfree(tme->text);
    tme->alloc_size = 0;
    
    tme->text = kmalloc(size, GFP_KERNEL);
    if(!tme->text) return -ENOMEM;
    
    tme->alloc_size = size;
    
    return 0;
}

//Initial buffer size
unsigned long buffer_size = BUFFER_SIZE_DEFAULT;
module_param(buffer_size, ulong, S_IRUGO);

// Names of files
static struct dentry* trace_file;
static struct dentry* trace_session_file;
static struct dentry* trace_dir;
static struct dentry* reset_file;
static struct dentry* buffer_size_file;
static struct dentry* lost_messages_file;

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

/* Helper for print into the buffer of limited size. */
struct print_buffer
{
    char* dest;
    size_t size;
    int size_written; /* Value for return from pretty print function. */
};

#define PRINT_BUFFER(name, dest, size) struct print_buffer name = {dest, size, 0}

/* Return number of written characters.
 * 
 * For compute return value of 'kedr_trace_pp_function' */
static inline int print_buffer_size_written(struct print_buffer* pb)
{
    return pb->size_written;
}

/* 
 * Print into print_buffer, pointed by 'pb', using snprintf-like
 * function 'fn'.
 * 
 * 'fn' is expected to accept 'dest' and 'size' as first 2 arguments
 * and return number of characters to be written(exclude '\0').
 */
#define snprintf_into_buffer(pb, fn, ...) do {\
    (pb)->size_written += fn((pb)->dest + (pb)->size_written, \
        (pb)->size > (pb)->size_written ? (pb)->size - (pb)->size_written: 0, \
        __VA_ARGS__); } while(0)

/* Print given arguments into print_buffer using given format. */
static void print_into_buffer(struct print_buffer* pb, const char* format, ...)
__printf(2, 0);

/* Print given string into print_buffer. */
static void str_into_buffer(struct print_buffer* pb, const char* str);

/************************ Auxiliary functions *************************/
/* 
 * Clear last message extracted.
 *
 * Should be executed under mutex taken.
 */
static void tme_last_clear(void)
{
    tme_clear(&tme_last);
    tme_last_current_pos = 0;
}

/* Reset trace. */
static void kedr_trace_reset(void)
{
    mutex_lock(&trace_m);
    trace_buffer_reset(tb_global);
    tme_last_clear();
    mutex_unlock(&trace_m);
}

/*
 * Trace marker for event about target loading/unloading.
 */
struct target_marker_data
{
    const char* target_name;
    bool is_loaded;
};

static int
kedr_trace_marker_target_pp_function(char* dest, size_t size,
    const void* data)
{
    const struct target_marker_data* tmd = data;
    
    return snprintf(dest, size, "target_%s: \"%s\"",
        tmd->is_loaded? "loaded" : "unloaded",
        tmd->target_name);
}

/* Add message about target module being loaded/unloaded. */
static void kedr_trace_marker_target(const char* target_name,
    bool is_loaded)
{
    struct target_marker_data* tmd;
    void* id = kedr_trace_lock(kedr_trace_marker_target_pp_function,
        sizeof(*tmd), (void**)&tmd);
    
    if(id)
    {
        tmd->target_name = target_name;
        tmd->is_loaded = is_loaded;
        
        kedr_trace_unlock_commit(id);
    }
}


/*
 * Trace marker for event about session start/end.
 */
struct session_marker_data
{
    bool is_started;
};

static int
kedr_trace_marker_session_pp_function(char* dest, size_t size,
    const void* data)
{
    const struct session_marker_data* smd = data;
    
    return snprintf(dest, size, "session_%s",
        smd->is_started? "started" : "ended");
}

/* Add message about session started/ended. */
static void kedr_trace_marker_session(bool is_started)
{
    struct session_marker_data* smd;
    void* id = kedr_trace_lock(kedr_trace_marker_session_pp_function,
        sizeof(*smd), (void**)&smd);
    
    if(id)
    {
        smd->is_started = is_started;
        
        kedr_trace_unlock_commit(id);
    }
}
   
/**************** Implementation of the exported functions ************/

/*
 * Unregister pretty print for data.
 * Clean all data from the trace.
 */
void kedr_trace_pp_unregister(void)
{
    kedr_trace_reset();
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
    void* id = trace_buffer_write_lock(tb_global,
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
    trace_buffer_write_unlock(tb_global, id);
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

void kedr_trace_call_after_read(kedr_trace_callback_func func,
    struct kedr_trace_callback_head* callback_head)
{
    trace_buffer_call_after_read(tb_global, func, callback_head);
}
EXPORT_SYMBOL(kedr_trace_call_after_read);

/* 
 * Information about target module.
 * 
 * Used for pretty print trace message.
 */
struct target_info
{
	struct list_head list;

	void* core_addr;
	void* init_addr;
	unsigned int core_size;
	unsigned int init_size;

	char name[MODULE_NAME_LEN];
	struct module* m;
    
    struct kedr_trace_callback_head callback_head;
};

void free_target_info_callback(struct kedr_trace_callback_head* ch)
{
    struct target_info* ti = container_of(ch, typeof(*ti), callback_head);
    
    kfree(ti);
}

/*
 * List of informations about currently loaded targets.
 *
 * Read and write are sync using RCU(preemt type) plus
 * kedr_trace_call_after_read mechanism.
 *
 * Writes are performed only at target_load/target_unload events,
 * which are already sync wrt themselves.
 */
static LIST_HEAD(targets_list);

/* Data for function call message. */
struct function_call_data
{
    const char* function_name;
    void* return_address;
    
    /* 
     * Information about target module which calls given function.
     * 
     * May be NULL.
     */
    struct target_info* ti;
    
    kedr_trace_pp_function params_pp;
    char params[0];
};

static bool within(void* addr, void* section_start,
    unsigned int section_size)
{
    return ((unsigned long)addr >= (unsigned long)section_start)
        && ((unsigned long)addr < ((unsigned long)section_start + section_size));
}

static int function_call_pp_function(char* dest, size_t size,
    const void* data)
{
    const struct function_call_data* fcd = data;
    PRINT_BUFFER(pb, dest, size);
    
    print_into_buffer(&pb, "called_%s: ", fcd->function_name);
    if(fcd->ti)
    {
        unsigned int rel_addr;
        struct target_info* ti = fcd->ti;
        bool is_core = within(fcd->return_address,
            ti->core_addr, ti->core_size);
        rel_addr = (unsigned int)((unsigned long)fcd->return_address
            - (is_core ? (unsigned long)ti->core_addr : (unsigned long)ti->init_addr));
        
        print_into_buffer(&pb, "([<%p>] %s.%s+0x%x)", fcd->return_address,
            ti->name, is_core? "core": "init", rel_addr);
    }
    else
    {
        print_into_buffer(&pb, "([%p])", fcd->return_address);
    }
    
    
    if(fcd->params_pp)
    {
        str_into_buffer(&pb, " ");
        snprintf_into_buffer(&pb, fcd->params_pp, fcd->params);
    }
    
    return print_buffer_size_written(&pb);
}

void* kedr_trace_function_call_lock(const char* function_name,
	void* return_address, kedr_trace_pp_function params_pp,
    size_t params_size, void** params)
{
    struct function_call_data* fcd;
    size_t size = offsetof(typeof(*fcd), params) + params_size;
    void* id = kedr_trace_lock(&function_call_pp_function, size, (void**)&fcd);
    
    if(id)
    {
        struct target_info* ti;
        list_for_each_entry_rcu(ti, &targets_list, list)
        {
            if(within(return_address, ti->core_addr, ti->core_size)
                || within(return_address, ti->init_addr, ti->init_size))
                break;
        }
        
        if(&ti->list == &targets_list) ti = NULL;
        
        fcd->function_name = function_name;
        fcd->return_address = return_address;
        fcd->ti = ti;
        fcd->params_pp = params_pp;

        *params = fcd->params;
    }
    
    return id;
}
EXPORT_SYMBOL(kedr_trace_function_call_lock);

void kedr_trace_function_call(const char* function_name,
	void* return_address, kedr_trace_pp_function params_pp,
	const void* params, size_t params_size)
{
    void* vparams;
    void* id = kedr_trace_function_call_lock(function_name, return_address,
        params_pp, params_size, &vparams);
    
    if(id)
    {
        if(params_size) memcpy(vparams, params, params_size);

        kedr_trace_unlock_commit(id);
    }
}
EXPORT_SYMBOL(kedr_trace_function_call);

static void on_target_loaded(struct module* target_module)
{
    struct target_info* ti = kmalloc(sizeof(*ti), GFP_KERNEL);
    
    if(!ti)
    {
        pr_err("Failed to allocate target's information struct\n.");
        return;
    }
    
    ti->core_addr = module_core_addr(target_module);
    ti->init_addr = module_init_addr(target_module);
    ti->core_size = core_size(target_module);
    ti->init_size = init_size(target_module);
    
    memcpy(ti->name, module_name(target_module), sizeof(ti->name));
    ti->m = target_module;
    
    list_add_rcu(&ti->list, &targets_list);
    
    kedr_trace_marker_target(ti->name, 1);
}

static void on_target_unloaded(struct module* target_module)
{
    struct target_info* ti;
    list_for_each_entry(ti, &targets_list, list)
    {
        if(ti->m == target_module) break;
    }
    
    if(&ti->list == &targets_list) return;
    
    kedr_trace_marker_target(ti->name, 0);
    
    list_del_rcu(&ti->list);
    
    /* 
     * Syncronize all currently writing messages for safety.
     * 
     * Normally, all messages referred to given 'ti' instance are
     * already commited.
     */
    synchronize_sched();
    
    kedr_trace_call_after_read(&free_target_info_callback,
        &ti->callback_head);
}

static void on_session_start(void)
{
    kedr_trace_marker_session(1);
}

static void on_session_end(void)
{
    kedr_trace_marker_session(0);
    /* Ignore return value. That is, session ending may fail silently. */
    if(kedr_trace_end_session())
    {
        pr_err("KEDR trace session failed to be ended.\n");
        // Ignore error status (except message printed above);
    }
}

static struct kedr_payload payload =
{
    .mod = THIS_MODULE,

    .on_session_start = &on_session_start,
    .on_session_end = &on_session_end,
    .on_target_loaded = &on_target_loaded,
    .on_target_about_to_unload = &on_target_unloaded
};


/**********************************************************************/
/*
 * Interpretator of the trace content as a string.
 */
static int trace_print_message(char* str, size_t size,
    const void* msg, size_t msg_size, int cpu, u64 ts)
{
    PRINT_BUFFER(pb, str, size);
    
    const struct kedr_trace_message* msg_real =
        (const struct kedr_trace_message*)msg;
    // ts is time in nanoseconds since system starts
    u32 sec, ms;
   
    sec = div_u64_rem(ts, 1000000000, &ms);
    ms /= 1000;

    print_into_buffer(&pb, "%s-%d\t[%.03d]\t%lu.%.06u:\t",
        msg_real->command, msg_real->pid,
        cpu, (unsigned long)sec, (unsigned)ms);
    
    snprintf_into_buffer(&pb, msg_real->pp, msg_real->data);
    
    str_into_buffer(&pb, "\n");

    return print_buffer_size_written(&pb);
}

/* Interpretator function for trace buffer. */
int trace_process_msg(const void* msg,
    size_t msg_size, int cpu, u64 ts, void* user_data)
{
    size_t read_size;
    int err;
    struct trace_session* session;
    
    BUG_ON(tme_last.text_size);
    BUG_ON(tme_last.message_session);
    
    /* It should be at most 2 iterations. */
    while(1)
    {
        read_size = trace_print_message(tme_last.text, tme_last.alloc_size,
            msg, msg_size, cpu, ts);
        
        if(read_size < tme_last.alloc_size) break;
        /* Text buffer is too small for message extracted. */
        err = tme_alloc(&tme_last, read_size + 1);
        if(err) return err;
    }

    tme_last.text_size = read_size;

    /* 
     * Look for session for message extracted.
     * Normally, it is the first element in the list.
     */
    list_for_each_entry(session, &trace_session_list, list)
    {
        if(!session->is_ended || (session->ts_end >= ts)) break;
    }
    
    BUG_ON(&session->list == &trace_session_list);
    
    tme_last.message_session = trace_session_ref(session);

    return 1; /* Message is assumed to be consumed. */
}

/* 
 * Read next chunk of the trace.
 * 
 * Call 'read_fn' for trace chunk [chunk, chunk + chunk_size).
 * If function return positive value, it is interpreted as number of
 * bytes read from the trace.
 * 
 * Return what 'read_fn' returns.
 * If read_session_p and *read_session_p are not NULL, value pointer
 * by 'read_session_p' parameter is interpreted
 * as session filter for message read. Also, in that case 0 is returned
 * instead of -EAGAIN in case of empty trace
 * (see 'struct trace_session' description above).
 * 
 * NOTE: read_session_p is dereferenced under mutex locked.
 */
static int trace_read(int (*read_fn)(const char* chunk,
    int chunk_size, struct trace_session* message_session, void* user_data),
    struct trace_session** read_session_p,
    void* user_data)
{
    int err;
    
    if(mutex_lock_interruptible(&trace_m))
        return -ERESTARTSYS;
    
    if(!tme_last.text_size)
    {
        /* Need to read next message from the trace buffer.*/
        err = trace_buffer_read(tb_global, &trace_process_msg, &tme_last);
        BUG_ON(!err); //Currently trace_process_msg print at least on character.
        if(err < 0)
        {
            if(err == -EAGAIN
                && read_session_p
                && *read_session_p
                && (*read_session_p)->is_ended)
            {
                err = 0;  //If read session is ended, trace emptiness means EOF.
            }
            goto out;
        } 
    }
    
    if(read_session_p
        && *read_session_p
        && *read_session_p != tme_last.message_session)
    {
        /* Message from another session, interpreted as EOF.*/
        err = 0;
        goto out;
    }
    
    err = read_fn(tme_last.text + tme_last_current_pos,
        tme_last.text_size - tme_last_current_pos, tme_last.message_session,
        user_data);
    if(err > 0)
    {
        tme_last_current_pos += err;
        
        if(tme_last_current_pos == tme_last.text_size)
        {
            /* Plain message is fully consumed. Clear it. */
            tme_last_clear();
        }
    }

out:
    mutex_unlock(&trace_m);
    
    return err;
}


/******************** Trace file operations ***************************/
struct read_fn_normal_data
{
    char __user* buf;
    size_t count;
    size_t bytes_read;
};

static int read_fn_normal(const char* chunk,
    int chunk_size, struct trace_session* session, void* user_data)
{
    struct read_fn_normal_data* read_data = user_data;
    int err;
    if(chunk_size > read_data->count - read_data->bytes_read)
        chunk_size = read_data->count - read_data->bytes_read;
    
    err = copy_to_user(read_data->buf + read_data->bytes_read, chunk, chunk_size);
    if(err) return -EFAULT;
    read_data->bytes_read += chunk_size;
    
    return chunk_size;
}

static ssize_t trace_file_op_read(struct file* filp, char __user* buf,
    size_t count, loff_t* f_pos)
{
    int err;
    struct read_fn_normal_data read_data;
    
    if(!count) return 0;
    
    read_data.buf = buf;
    read_data.count = count;
    read_data.bytes_read = 0;
    
    err = trace_read(read_fn_normal, NULL, &read_data);
    if(err == -EAGAIN && !(filp->f_flags & O_NONBLOCK))
    {
        bool woken_flag = 0;
        DEFINE_WAIT_NESTED(w_buffer, woken_flag);
        
        wait_queue_head_t* wq_buffer = trace_buffer_get_wait_queue(tb_global);
        
        while(1)
        {
            add_wait_queue_nestable(wq_buffer, &w_buffer);

            err = trace_read(read_fn_normal, NULL, &read_data);
            
            if(err != -EAGAIN) break;
            err = wait_flagged_interruptible(&woken_flag);
            if(err) break;
        }
        
        remove_wait_queue_nestable(wq_buffer, &w_buffer);
    }
    if(err < 0) return err;
    
    while(read_data.bytes_read < read_data.count)
    {
        err = trace_read(read_fn_normal, NULL, &read_data);
        if(err < 0) break;
    }
    
    return read_data.bytes_read;
}

static int poll_fn(const char* chunk,
    int chunk_size, struct trace_session* session, void* user_data)
{
    return 0;
}

static unsigned int trace_file_op_poll(struct file *filp, poll_table *wait)
{
    int err;
    
    wait_queue_head_t* wq_buffer = trace_buffer_get_wait_queue(tb_global);
    poll_wait(filp, wq_buffer, wait);
    
    err = trace_read(&poll_fn, NULL, NULL);
    
    switch(err)
    {
    case 0:
        return POLLIN | POLLRDNORM;
    case -EAGAIN:
        return 0;
    default:
        return (unsigned)err;
    }
}

static int trace_file_op_open(struct inode* inode, struct file* filp)
{
    return nonseekable_open(inode, filp);
}

static struct file_operations trace_file_ops =
{
    .owner = THIS_MODULE,
    .open = &trace_file_op_open,
    .read = &trace_file_op_read,
    .poll = &trace_file_op_poll
};
/******************** trace_session file operations *******************/
struct read_fn_session_data
{
    struct read_fn_normal_data normal_data;
    struct trace_session** session_p;
};

static int read_fn_session(const char* chunk,
    int chunk_size, struct trace_session* session, void* user_data)
{
    struct read_fn_session_data* read_data = user_data;
    int err;
    
    err = read_fn_normal(chunk, chunk_size, session, &read_data->normal_data);
    if(err > 0)
    {
        // Setup our sesson if it is not set.
        if(!*read_data->session_p)
        {
            *read_data->session_p = trace_session_ref(session);
        }
    }    

    return err;
}

static ssize_t trace_file_session_op_read(struct file* filp, char __user* buf,
    size_t count, loff_t* f_pos)
{
    int err;
    struct read_fn_session_data read_data;
    struct trace_session** read_session_p = (struct trace_session**)&filp->private_data;
    
    if(!count) return 0;
    
    read_data.normal_data.buf = buf;
    read_data.normal_data.count = count;
    read_data.normal_data.bytes_read = 0;
    read_data.session_p = read_session_p;
    
    err = trace_read(read_fn_session, read_session_p, &read_data);
    if(err == -EAGAIN && !(filp->f_flags & O_NONBLOCK))
    {
        bool woken_flag = 0;
        DEFINE_WAIT_NESTED(w_buffer, woken_flag);
        DEFINE_WAIT_NESTED(w_session, woken_flag);
        
        wait_queue_head_t* wq_buffer = trace_buffer_get_wait_queue(tb_global);
        
        while(1)
        {
            add_wait_queue_nestable(wq_buffer, &w_buffer);
            add_wait_queue_nestable(&wq_session, &w_session);
            err = trace_read(read_fn_session, read_session_p, &read_data);
            
            if(err != -EAGAIN) break;
            err = wait_flagged_interruptible(&woken_flag);
            if(err) break;
        }
        
        remove_wait_queue_nestable(wq_buffer, &w_buffer);
        remove_wait_queue_nestable(&wq_session, &w_session);
    }
    if(err <= 0) return err;
    
    while(read_data.normal_data.bytes_read < read_data.normal_data.count)
    {
        err = trace_read(read_fn_session, read_session_p, &read_data);
        if(err <= 0) break;
    }
    
    return read_data.normal_data.bytes_read;
}

static unsigned int trace_file_session_op_poll(struct file *filp, poll_table *wait)
{
    int err;
    
    struct trace_session** read_session_p = (struct trace_session**)&filp->private_data;
    wait_queue_head_t* wq_buffer = trace_buffer_get_wait_queue(tb_global);
    poll_wait(filp, wq_buffer, wait);
    poll_wait(filp, &wq_session, wait);
    
    err = trace_read(&poll_fn, read_session_p, NULL);
    
    switch(err)
    {
    case 0:
        return POLLIN | POLLRDNORM;
    case -EAGAIN:
        return 0;
    default:
        return (unsigned)err;
    }
}

static int trace_file_session_op_open(struct inode* inode, struct file* filp)
{
    // Currently, this is perfomed by VFS automatically, but nevertheless.
    filp->private_data = NULL;
    
    return nonseekable_open(inode, filp);
}

static int trace_file_session_op_release(struct inode* inode, struct file* filp)
{
    struct trace_session* read_session = filp->private_data;
    if(read_session) trace_session_unref(read_session);
    
    return 0;
}

static struct file_operations trace_file_session_ops =
{
    .owner = THIS_MODULE,
    .open = &trace_file_session_op_open,
    .read = &trace_file_session_op_read,
    .poll = &trace_file_session_op_poll,
    .release = &trace_file_session_op_release,
};
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
        kedr_trace_reset();
    }
    return nonseekable_open(inode, filp);
}

static struct file_operations reset_file_ops = 
{
    .owner = THIS_MODULE,
    .open = &reset_file_open,
    .write = &reset_file_write,
};

// Buffer size file operations implementation
static int buffer_size_seq_show(struct seq_file* m, void* v)
{
    int err = mutex_lock_interruptible(&trace_m);
    if(err) return err;
    
    seq_printf(m, "%lu\n", (unsigned long)trace_buffer_size(tb_global));
    
    mutex_unlock(&trace_m);
    
    return 0;
}

static int
buffer_size_file_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &buffer_size_seq_show, NULL);
}

static ssize_t
buffer_size_file_write(struct file *filp,
    const char __user *buf, size_t count, loff_t * f_pos)
{
    int err = 0;
    unsigned long size;
    
    if(count == 0) return -EINVAL;
    {
        char* str = kmalloc(count + 1, GFP_KERNEL);
        if(str == NULL) return -ENOMEM;

        if(copy_from_user(str, buf, count))
        {
            kfree(str);
            return -EFAULT;
        }
        str[count] = '\0';

        err = kstrtoul(str, 0, &size);

        kfree(str);
        if(err) return err;
    }
    
    err = mutex_lock_interruptible(&trace_m);
    if(err) return err;
    err = trace_buffer_resize(tb_global, size);
    if(err) goto out;
    
    tme_last_clear();
out:
    mutex_unlock(&trace_m);
    
    return err ? err : count;
}

static struct file_operations buffer_size_file_ops = 
{
    .owner = THIS_MODULE,
    .open = &buffer_size_file_open,
    .read = &seq_read,
    .write = &buffer_size_file_write,
    .release = &single_release
};


// Lost messages file operations implementation
static int lost_messages_seq_show(struct seq_file* m, void* v)
{
    int err = mutex_lock_interruptible(&trace_m);
    if(err) return err;
    
    seq_printf(m, "%lu\n", (unsigned long)trace_buffer_lost_messages(tb_global));
    
    mutex_unlock(&trace_m);
    
    return 0;
}

static int
lost_messages_file_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &lost_messages_seq_show, NULL);
}

static struct file_operations lost_messages_file_ops = 
{
    .owner = THIS_MODULE,
    .open = &lost_messages_file_open,
    .read = &seq_read,
    .release = &single_release,
};


void print_into_buffer(struct print_buffer* pb, const char* format, ...)
{
    va_list arg;
    
    va_start(arg, format);
    pb->size_written += vsnprintf(pb->dest + pb->size_written,
        pb->size > pb->size_written? pb->size - pb->size_written: 0,
        format,
        arg);
    va_end(arg);
}

void str_into_buffer(struct print_buffer* pb, const char* str)
{
    size_t len = strlen(str);
    
    if(pb->size > pb->size_written)
    {
        size_t cp_size = pb->size - pb->size_written;
        if(cp_size > len) cp_size = len;
        
        memcpy(pb->dest + pb->size_written, str, cp_size);
    }
    pb->size_written += len;
}

/************************ Module definition ***************************/
static int __init
kedr_trace_module_init(void)
{
    int err = -ENOMEM;
    struct trace_session* first_session;

    tme_init(&tme_last);
    
    first_session = trace_session_create();
    if(!first_session) goto fail_trace_session;
    list_add(&first_session->list, &trace_session_list);

    tb_global = trace_buffer_alloc(buffer_size, 1);
    if(!tb_global) goto fail_trace_buffer;
    
    trace_dir = debugfs_create_dir("kedr_tracing", NULL);
    if(!trace_dir) goto fail_trace_dir;
    
    trace_file = debugfs_create_file("trace", S_IRUSR, trace_dir,
        NULL, &trace_file_ops);
        
    if(!trace_file) goto fail_trace_file;

    trace_session_file = debugfs_create_file("trace_session", S_IRUSR, trace_dir,
        NULL, &trace_file_session_ops);
        
    if(!trace_session_file) goto fail_trace_session_file;

    reset_file = debugfs_create_file("reset",
        S_IWUSR | S_IWGRP,
        trace_dir,
        NULL,
        &reset_file_ops);

    if(!reset_file) goto fail_reset_file;

    buffer_size_file = debugfs_create_file("buffer_size",
        S_IRUGO | S_IWUSR,
        trace_dir,
        NULL,
        &buffer_size_file_ops);
        
    if(!buffer_size_file) goto fail_buffer_size_file;
    
    lost_messages_file = debugfs_create_file("lost_messages",
        S_IRUGO,
        trace_dir,
        NULL,
        &lost_messages_file_ops);
    
    if(!lost_messages_file) goto fail_lost_messages_file;

    err = kedr_payload_register(&payload);
    if(err) goto fail_payload;

    return 0;

fail_payload:
    debugfs_remove(lost_messages_file);
fail_lost_messages_file:
    debugfs_remove(buffer_size_file);
fail_buffer_size_file:
    debugfs_remove(reset_file);
fail_reset_file:
    debugfs_remove(trace_session_file);
fail_trace_session_file:
    debugfs_remove(trace_file);
fail_trace_file:
    debugfs_remove(trace_dir);
fail_trace_dir:
    trace_buffer_destroy(tb_global);
fail_trace_buffer:
    list_del(&first_session->list);
    trace_session_unref(first_session);
fail_trace_session:
    tme_destroy(&tme_last);
    return err;

}

static void __exit
kedr_trace_module_exit(void)
{
    struct trace_session* first_session;
    
    kedr_payload_unregister(&payload);
    debugfs_remove(lost_messages_file);
    debugfs_remove(buffer_size_file);
    debugfs_remove(reset_file);
    debugfs_remove(trace_session_file);
    debugfs_remove(trace_file);
    debugfs_remove(trace_dir);
    trace_buffer_destroy(tb_global);

    first_session = list_first_entry(&trace_session_list,
        typeof(*first_session), list);
    list_del(&first_session->list);
    BUG_ON(!list_empty(&trace_session_list));
    trace_session_unref(first_session);
    
    tme_destroy(&tme_last);
}

module_init(kedr_trace_module_init);
module_exit(kedr_trace_module_exit);

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_DESCRIPTION("KEDR tracing system");
MODULE_LICENSE("GPL");
