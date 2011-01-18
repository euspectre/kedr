/*
 * Implementation of the 'trace_buffer' API.
 */
#include "trace_buffer.h"

#include <linux/ring_buffer.h> /* ring buffer functions*/
#include <linux/delay.h> /* msleep_interruptible definition */

#include <linux/cpumask.h> /* definition of 'struct cpumask'(cpumask_t)*/
#include <linux/threads.h> /*definition of NR_CPUS macro*/

#include <linux/mutex.h> /* mutexes */

#include <linux/slab.h> /* kmalloc and others*/

#include <linux/wait.h> /*wait queue definitions*/

#include <linux/sched.h> /* TASK_NORMAL, TASK_INTERRUPTIBLE*/

#include "config.h"
/*
 * Configurable parameters for internal implementation of the buffer.
 */

/* 
 * Problem: extracting message in bloking mode, but buffer is empty.
 * 
 * Decision: extractor wait for some time, and then perform new attempt
 * to read message from buffer.
 * 
 * 'TIME_WAIT_BUFFER' is a time in ms for this waiting.
 */
#define TIME_WAIT_BUFFER 500

/*
 * Problem: extracting message in blocking mode; there are some messages
 *   in the buffer with oldest timestamp 'ts_message',
 *   but there are also empty sub-buffer with last access timestamp 
 *   'ts_access', and 'ts_access'<'ts_message'.
 * 
 *   In this case one cannot say, that message with timestamp 'ts_message'
 *   WILL the oldest message IN ANY CASE besause message with timestamp
 *   'ts_access' may appear in future in the empty subbuffer.
 *
 * Descision: extractor wait a little, and then update access timestamp
 *   in the empty subbuffer(and try to read message from it).
 *   Even the subbuffer remain to be empty, last access timestamp may
 *   exceed 'ts_message', so ordering may be performed.
 * 
 * 'TIME_WAIT_SUBBUFFER' is a time in ms of this waiting.
 * 
 * Note: this time may be 0, because for normal counting of timestamps
 *   for CPUs, if timestamp 'ts_access' evaluated after
 *   'ts_message', then automatically ts_access >= ts_message.
 * 
 */
#define TIME_WAIT_SUBBUFFER 1

/*
 * Describe last message from per-cpu buffer.
 * 
 * Because this message may be long not accessed, we cannot store
 * it as ring_buffer_event.
 * 
 * There are two different types of this struct:
 * 
 * First - for existent last message:
 * event = ring_buffer_consume(buffer, cpu, &.ts);
 * 
 * .msg = ring_buffer_event_data(event),
 * .size = ring_buffer_event_length(event),
 * .ts - time stamp of the event.
 * 
 * Second - for empty per-cpu buffer:
 * .msg = NULL,
 * .size = 0,
 * .ts - time stamp, at which buffer was definitly empty.
 */
struct last_message
{
    int cpu;
    u64 ts;
    bool is_exist;
    
    void *msg;
    size_t size;
    
    struct list_head list;//ordered array of 'struct last_message'
};

static int last_message_init(struct last_message* message, int cpu, u64 ts)
{
    message->cpu = cpu;
    message->ts = ts;
    message->is_exist = 0;
    
    message->msg = NULL;
    message->size = 0;
    return 0;
}
static void
last_message_set_timestamp(struct last_message* message, u64 ts)
{
    message->ts = ts;
}
static int
last_message_set(struct last_message* message, struct ring_buffer_event* event)
{
    size_t size;
    BUG_ON(message->is_exist);
    size = ring_buffer_event_length(event);
    message->msg = krealloc(message->msg, size, GFP_KERNEL);
    if(message->msg == NULL)
    {
        message->size = 0;
        //Cannot allocate message for return - drop it
        pr_err("last_message_fill: Cannot allocate message from event.");
        //now simply return
        return -ENOMEM;
    }
    memcpy(message->msg,
        ring_buffer_event_data(event),
        size);
    message->size = size;
    message->is_exist = 1;
    return 0;
}

static void last_message_clear(struct last_message* message)
{
    message->is_exist = 0;
    // Do not free old message contents - for realloc
    //kfree(message->msg);
    //message->msg = NULL;
    //message->size = 0;
}

static void last_message_destroy(struct last_message* message)
{
    kfree(message->msg);
}


/*
 * Struct, represented buffer which support two main operations:
 *
 * 1.Writting message into buffer
 * 2.Extract oldest message from the buffer.
 *   If buffer is empty, may wait.
 */

struct trace_buffer
{
    struct ring_buffer* buffer;

    /*
     * Array of last messages from all possible CPUs
     */
    struct last_message last_messages[NR_CPUS];
    
    struct list_head last_messages_ordered;// from the oldest timestamp to the newest one
    //number of per-cpu buffers, from which messages was readed into 'struct last_message'
    int non_empty_buffers;
    
    /*
     * Prevent concurrent reading of messages.
     */
    struct mutex read_mutex;
    /*
     * Number of messages, lost due to incorrect processing
     * of the events from ring_buffer_consume().
     * Other losts are account in ring_buffer_overruns().
     */
    unsigned long messages_lost_internal;
    // Wait queue for reading and polling
    wait_queue_head_t rq;
    // Work in which reader will wake up.
    // This work shedule only on demand.
    struct delayed_work work_wakeup_reader;
};

static void wake_up_reader(struct work_struct *work)
{
    struct trace_buffer* trace_buffer = container_of(to_delayed_work(work), struct trace_buffer, work_wakeup_reader);
    //pr_info("Wake up all.");
    wake_up_all(&trace_buffer->rq);//unconditionally wakeup
}

/*
 * Clear all messages in the buffer.
 *
 * Should be executed with lock taken.
 */

static void trace_buffer_clear_internal(struct trace_buffer* trace_buffer)
{
    int cpu;
    //Clear last messages
    INIT_LIST_HEAD(&trace_buffer->last_messages_ordered);
    trace_buffer->non_empty_buffers = 0;

    for_each_possible_cpu(cpu)
    {
        struct list_head* insert_point;
        struct last_message* last_message =
            &trace_buffer->last_messages[cpu];
        u64 ts = ring_buffer_time_stamp(trace_buffer->buffer, cpu);
        last_message_clear(last_message);
        last_message_set_timestamp(last_message, ts);
        //look for position for insert entry into the list
        list_for_each_prev(insert_point, &trace_buffer->last_messages_ordered)
        {
            if(list_entry(insert_point, struct last_message, list)->ts <= ts) break;
        }
        list_add(&last_message->list, insert_point);
    }

    trace_buffer->messages_lost_internal = 0;
    ring_buffer_reset(trace_buffer->buffer);
}

/*
 * Allocate buffer.
 * 
 * 'size' is size of the buffer created.
 * 'mode_overwrite' determine policy,
 *  when size is overflowed while writting message:
 *   if 'mode_overwrite' is 0, then newest message will be dropped.
 *   otherwise the oldest message will be dropped.
 */
struct trace_buffer* trace_buffer_alloc(
    size_t size, bool mode_overwrite)
{
    int cpu;

    struct trace_buffer* trace_buffer = kmalloc(sizeof(*trace_buffer),
        GFP_KERNEL);
    
    if(trace_buffer == NULL)
    {
        pr_err("trace_buffer_alloc: Cannot allocate trace_buffer structure.");
        return NULL;
    }
    
    trace_buffer->buffer = ring_buffer_alloc(size,
        mode_overwrite? RB_FL_OVERWRITE : 0);
    if(trace_buffer->buffer == NULL)
    {
        pr_err("trace_buffer_alloc: Cannot allocate ring buffer.");
        kfree(trace_buffer);
        return NULL;
    }

    //Initialize array of the oldest messages from per-cpu buffers
    INIT_LIST_HEAD(&trace_buffer->last_messages_ordered);
    trace_buffer->non_empty_buffers = 0;
    for_each_possible_cpu(cpu)
    {
        struct list_head* insert_point;
        struct last_message* last_message =
            &trace_buffer->last_messages[cpu];
        u64 ts = ring_buffer_time_stamp(trace_buffer->buffer, cpu);
        //now last_message_init return only 0(success)
        last_message_init(last_message, cpu, ts);
        //look for position for insert entry into the list
        list_for_each_prev(insert_point, &trace_buffer->last_messages_ordered)
        {
            if(list_entry(insert_point, struct last_message, list)->ts <= ts) break;
        }
        list_add(&last_message->list, insert_point);
    }
    
    
    mutex_init(&trace_buffer->read_mutex);
    
    trace_buffer->messages_lost_internal = 0;
    
    init_waitqueue_head(&trace_buffer->rq);
    
    INIT_DELAYED_WORK(&trace_buffer->work_wakeup_reader, wake_up_reader);
    
    return trace_buffer;
}
/*
 * Destroy buffer, free all resources which it used.
 */
void trace_buffer_destroy(struct trace_buffer* trace_buffer)
{
    int cpu;
    
    cancel_delayed_work_sync(&trace_buffer->work_wakeup_reader);
    
    mutex_destroy(&trace_buffer->read_mutex);
    //May be cpu-s, for which bit in subbuffers_empty is set,
    //but which messages is not freed.
    //So iterate over all possible cpu's
    for_each_possible_cpu(cpu)
    {
        struct last_message* last_message =
            &trace_buffer->last_messages[cpu];
        
        last_message_destroy(last_message);
    }
    ring_buffer_free(trace_buffer->buffer);
    kfree(trace_buffer);
}

/*
 * Write message with data 'msg' of length 'size' to the buffer.
 * May be called in the atomic context.
 */
void trace_buffer_write_message(struct trace_buffer* trace_buffer,
    const void* msg, size_t size)
{
    //need to cast msg to non-constan pointer,
    // but really its content is not changed inside function
    ring_buffer_write(trace_buffer->buffer, size, (void*)msg);
}

/*
 * Reserve space in the buffer for writting message.
 * 
 * After call, pointer to the reserved space is saved in the 'msg'.
 * 
 * Return not-NULL identificator, which should be passed to
 * the trace_buffer_write_unlock() for commit writing.
 *
 * On error NULL is returned and 'msg' pointer shouldn't be used.
 * 
 * May be called in the atomic context.
 */
void* trace_buffer_write_lock(struct trace_buffer* trace_buffer,
    size_t size, void** msg)
{
    struct ring_buffer_event* event =
        ring_buffer_lock_reserve(trace_buffer->buffer, size);
    if(event == NULL) return NULL;
    *msg = ring_buffer_event_data(event);
    return event;
}

/*
 * Commit message written after previous call
 * trace_buffer_write_lock().
 * 
 * May be called in the atomic context.
 */
void trace_buffer_write_unlock(struct trace_buffer* trace_buffer,
    void* id)
{
    struct ring_buffer_event* event = (struct ring_buffer_event*)id;
    ring_buffer_unlock_commit(trace_buffer->buffer, event);
}


/*
 * Non-blocking read only current oldest message(without reading of the per-cpu buffers).
 *
 * Should be executed under lock.
 */

static int trace_buffer_read_internal(struct trace_buffer* trace_buffer,
    int (*process_data)(const void* msg, size_t size, int cpu,
        u64 ts, bool *consume, void* user_data),
    void* user_data)
{
    bool consume = 0;//do not consume message by default
    int result;
    // Determine oldest message
    struct last_message* oldest_message = 
        list_first_entry(&trace_buffer->last_messages_ordered, struct last_message, list);
    if(!oldest_message->is_exist)
    {
        return -EAGAIN;
    }

    result = process_data(oldest_message->msg,
        oldest_message->size,
        oldest_message->cpu,
        oldest_message->ts,
        &consume,
        user_data);
    //Remove oldest message if it is consumed
    if(consume)
    {
        last_message_clear(oldest_message);
        trace_buffer->non_empty_buffers--;
    }

    return result;
}

/*
 * Update oldest message if needed and possible.
 * May read every cpu-buffer, but not more then once.
 *
 * Should be executed under lock.
 *
 * Return 0 on success.
 * If cannot update oldest message, because need read some cpu-buffer more then once, return -EAGAIN.
 * Otherwise return negative error code.
 *
 * If 'wait_function' is not NULL, call it for waitqueue, which is woken up when new messages in the buffer
 * are available.
 */

static int trace_buffer_update_internal(struct trace_buffer* trace_buffer,
    void (*wait_function)(wait_queue_head_t* wq, void* data),
    void* data)
{
    struct last_message* oldest_message;
    cpumask_t subbuffers_updated;

    if(wait_function)
        wait_function(&trace_buffer->rq, data);
    
    cpumask_clear(&subbuffers_updated);
    // Try to determine oldest message in the buffer(from all cpu's)
    for(oldest_message = list_first_entry(&trace_buffer->last_messages_ordered, struct last_message, list);
        !oldest_message->is_exist;
        oldest_message = list_first_entry(&trace_buffer->last_messages_ordered, struct last_message, list))
    {
        // Cannot determine latest message - need to update timestamp
        int cpu = oldest_message->cpu;
        u64 ts;
        struct ring_buffer_event* event;
        struct list_head* insert_point;
        
        if(cpumask_test_cpu(cpu, &subbuffers_updated))
        {
            //This cpu-buffer has already been tested. Buffer is cannot read now.
            if(wait_function)
                schedule_delayed_work(&trace_buffer->work_wakeup_reader,
                    (trace_buffer->non_empty_buffers ? TIME_WAIT_SUBBUFFER : TIME_WAIT_BUFFER)
                    * HZ / 1000/*jiffies in ms*/);
            
            return -EAGAIN;
        }
        ts = ring_buffer_time_stamp(trace_buffer->buffer, cpu);
#if defined(RING_BUFFER_CONSUME_HAS_4_ARGS)
		event = ring_buffer_consume(trace_buffer->buffer, cpu, &ts, NULL);
#elif defined(RING_BUFFER_CONSUME_HAS_3_ARGS)
		event = ring_buffer_consume(trace_buffer->buffer, cpu, &ts);
#else
#error RING_BUFFER_CONSUME_HAS_4_ARGS or RING_BUFFER_CONSUME_HAS_3_ARGS should be defined.
#endif
        last_message_set_timestamp(oldest_message, ts);
        //rearrange 'oldest_message'
        list_del(&oldest_message->list);
        list_for_each_prev(insert_point, &trace_buffer->last_messages_ordered)
        {
            if(list_entry(insert_point, struct last_message, list)->ts <= ts) break;
        }
        list_add(&oldest_message->list, insert_point);
        //mark cpu as 'updated'
        cpumask_set_cpu(cpu, &subbuffers_updated);
        
        if(event)
        {
            if(last_message_set(oldest_message, event))
            {
                pr_err("Cannot allocate new message.");
                trace_buffer->messages_lost_internal++;
                return -ENOMEM;
            }
            trace_buffer->non_empty_buffers++;
        }
    }
   
    return 0;
}

//callback function for update_internal() for perform blocking read.
//For simplisity, it containt only one entry for record wait_queue_head_t and wait_queue_t pair.
struct read_wait_table
{
    wait_queue_t wait;
    wait_queue_head_t* q;
};

static void
read_wait_init(struct read_wait_table* table)
{
    table->q = NULL;
    init_wait(&table->wait);
}

static void
read_wait_finish(struct read_wait_table* table)
{
    if(table->q)
    {
        finish_wait(table->q, &table->wait);
        table->q = NULL;
    }
    else set_current_state(TASK_RUNNING);
}

static void
read_wait_function(wait_queue_head_t* q, void* data)
{
    struct read_wait_table* table = (struct read_wait_table*)data;
    BUG_ON(table->q != NULL);
    table->q = q;
    add_wait_queue(q, &table->wait);
}

/*
 * Read the oldest message from the buffer, and consume it.
 * 
 * For message consumed call 'process_data':
 * 'msg' is set to the pointer to the message data.
 * 'size' is set to the size of the message,
 * 'cpu' is set to the cpu, on which message was written,
 * 'ts' is set to the timestamp of the message,
 * 'user_data' is set to the 'user_data' parameter of the function.
 * 
 * Return value, which is returned by 'process_data'.
 * 
 * If buffer is empty, and should_wait is 0,
 * return 0; otherwise wait until message will be available
 * 
 * If error occures, return negative error code.
 * 
 * Shouldn't be called in atomic context.
 */

int
trace_buffer_read_message(struct trace_buffer* trace_buffer,
    int (*process_data)(const void* msg, size_t size, int cpu,
        u64 ts, bool *consume, void* user_data),
    int should_wait,
    void* user_data)
{
    int result;
    if(mutex_lock_killable(&trace_buffer->read_mutex))
        return -ERESTARTSYS;

    
    //pr_info("Updating buffers");
    while(((result = trace_buffer_update_internal(trace_buffer, NULL, NULL)) == -EAGAIN)
        && should_wait)
    {
        //perform wait_event_killable()
        struct read_wait_table table;
        read_wait_init(&table);
        //pr_info("Waiting trace...");
        while(1)
        {
            set_current_state(TASK_KILLABLE);
            //verify condition again, but with registering reader on waitqueue.
            result = trace_buffer_update_internal(trace_buffer, read_wait_function, &table);
            if(result != -EAGAIN) break;
            //verify signal
            if(fatal_signal_pending(current))
            {
                result = -ERESTARTSYS;
                break;
            }
            mutex_unlock(&trace_buffer->read_mutex);
            //drop lock before scheduling...
            schedule();
            //and reaquire it
            if(mutex_lock_killable(&trace_buffer->read_mutex))
            {
                result = -ERESTARTSYS;
                break;
            }
            read_wait_finish(&table);
        }
        read_wait_finish(&table);
        if(result != -EAGAIN) break;
    }
    if(result)
        goto out;
    //pr_info("Reading message");
    result = trace_buffer_read_internal(trace_buffer, process_data, user_data);
out:
    mutex_unlock(&trace_buffer->read_mutex);
    return result;
}

/*
 * Polling read status of trace_buffer.
 *
 * wait_function should have semantic similar to poll_wait().
 *
 * Return 1 if buffer can currently be read without lock, 0 otherwise.
 */
int
trace_buffer_poll_read(struct trace_buffer* trace_buffer,
    void (*wait_function)(wait_queue_head_t* wq, void* data),
    void* data)
{
    int result;

    if(mutex_lock_killable(&trace_buffer->read_mutex))
        return -ERESTARTSYS;
    result = trace_buffer_update_internal(trace_buffer, wait_function, data);
    mutex_unlock(&trace_buffer->read_mutex);
    if(result == 0)
        return 1;//buffer may currently be read without lock
    else if(result == -EAGAIN)
        return 0;//buffer may not currently be read without lock
    else
        return result;//error
}


/*
 * Return number of messages lost due to the buffer overflow.
 */

unsigned long
trace_buffer_lost_messages(struct trace_buffer* trace_buffer)
{
    return ring_buffer_overruns(trace_buffer->buffer)
            + trace_buffer->messages_lost_internal;
}

/*
 * Reset trace in the buffer.
 * 
 * Return 0 on success, negative error code otherwise.
 */
int
trace_buffer_reset(struct trace_buffer* trace_buffer)
{
    if(mutex_lock_interruptible(&trace_buffer->read_mutex))
    {
        return -ERESTARTSYS;
    }
    trace_buffer_clear_internal(trace_buffer);
    mutex_unlock(&trace_buffer->read_mutex);
    return 0;
}

/*
 * Return size of buffer in bytes.
 */
unsigned long
trace_buffer_size(struct trace_buffer* trace_buffer)
{
    return ring_buffer_size(trace_buffer->buffer);
}

/*
 * Change size of the buffer.
 *
 * Current messages in the buffer may be silently lost.
 * (in current implementation buffer is forcibly reseted).
 *
 * Return new size on success, negative error code otherwise.
 */

int trace_buffer_resize(struct trace_buffer* trace_buffer,
    unsigned long size)
{
    int result;
    if(mutex_lock_interruptible(&trace_buffer->read_mutex))
    {
        return -ERESTARTSYS;
    }
    result = ring_buffer_resize(trace_buffer->buffer, size);
    trace_buffer_clear_internal(trace_buffer);
    
    mutex_unlock(&trace_buffer->read_mutex);
    return result;
}
