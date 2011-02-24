/*
 * Implementation of the 'trace_buffer' API.
 */
#include "trace_buffer.h"

#include <linux/ring_buffer.h> /* ring buffer functions*/

#include <linux/cpumask.h> /* definition of 'struct cpumask'(cpumask_t)*/
#include <linux/threads.h> /*definition of NR_CPUS macro*/

#include <linux/mutex.h> /* mutexes */

#include <linux/slab.h> /* kmalloc and others*/

#include <linux/wait.h> /*wait queue definitions*/

#include <linux/sched.h> /* TASK_NORMAL, TASK_INTERRUPTIBLE*/

#include <linux/hardirq.h> /* in_nmi() */

#include <linux/hrtimer.h> /* high resolution timer for clock*/

#include "config.h"

/*
 * For some reason, standard clock for ring buffer is not sufficient
 * for interprocessor message ordering. So, need to implement our clock.
 * 
 * The function, represented clock, should:
 * 1)on one processor, be monotonic function from the real time.
 * 2)when called one different processors at the nearest the same time,
 * return near values.
 * 3)be convertable(with some precision) to the real time, passed from
 * the system starts.
 * 4)have precision which allows correct ordering of messages on
 * different CPUs, which represent strictly ordered actions
 * (such as spin_unlock() and spin_lock(), or kfree() and kmalloc()).
 * 5)do not overflow in several days.
 * 
 * NOTE: correct ordering of messages, generated on one CPU,
 *  is not required.
 * 
 * Points 4,5 may be violated in not-release versions.
 * 
 */

/*
 * We want timestamps to be strongly monotonic.
 */
static u64 last_ts = 0;
static DEFINE_SPINLOCK(last_ts_lock);

static u64 correct_ts(u64 ts)
{
    unsigned long flags;

    /*
     * If in an NMI context then dont risk lockups and return the
     * input timestamp:
     */

    if (in_nmi()) return ts;
    
    spin_lock_irqsave(&last_ts_lock, flags);

    if ((s64)(ts - last_ts) <= 0)
        ts = last_ts + 1;
    last_ts = ts;

    spin_unlock_irqrestore(&last_ts_lock, flags);

    return ts;
}

static u64
kedr_clock(void)
{
    //should be correct with garantee
    return correct_ts(ktime_to_ns(ktime_get()));
}

/*
 * Maximum time which may go since message timestamping with our clock
 * until message will become available for read (ring_buffer_consume).
 * 
 * Error in this time may lead in some message cross-cpu disordering
 * when read is concurrent with write.
 */

#define KEDR_CLOCK_DELTA 100

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
 * .ts - timestamp(with out clock) of the message
 *
 * .msg - content of the message
 * .size - size of the message
 * 
 * Second - for empty corresponded per-cpu buffer:
 * .ts - time stamp, at which buffer was definitly empty.
 * 
 * .msg and .size may be NULL and 0 correspondingly,
 * or point to the message, which was already consumed, but not freed.
 * .
 */
struct last_message
{
    u64 ts;
    
    void *msg;
    size_t size;
    
    struct list_head list;//ordered array of 'struct last_message'
};

struct last_message_array
{
    //Array of 'last_message' content for corresponding CPUs.
    struct last_message messages[NR_CPUS];
    //mask describing per-cpu buffers with existing messages.
    cpumask_t message_exist;
    //List organization of 'last_messages', ordered by .ts in ascended order.
    struct list_head messages_ordered;
};

//Insert last_message into the list, taking time ordering into account.
static void last_message_list_add(struct list_head* messages,
    struct last_message* message)
{
    struct last_message* insert_point = NULL, *point;
    list_for_each_entry_reverse(point, messages, list)
    {
        if(point->ts <= message->ts)
        {
            insert_point = point;
            break;
        }
    }
    if(insert_point)
        list_add(&message->list, &insert_point->list);
    else
        list_add(&message->list, messages);
}


static int
last_message_array_init_cpu(struct last_message_array* array,
    int cpu, u64 ts)
{
    struct last_message* message = &array->messages[cpu];
    message->msg = NULL;
    message->ts = ts;
    
    last_message_list_add(&array->messages_ordered,
        message);
    return 0;
}
static int
last_message_array_init(struct last_message_array* array, u64 ts)
{
    int cpu;
    INIT_LIST_HEAD(&array->messages_ordered);
    cpumask_clear(&array->message_exist);
    for_each_possible_cpu(cpu)
        last_message_array_init_cpu(array, cpu, ts);
    return 0;
}

/*
 * Update timestamp of unexistent message.
 */
static void
last_message_array_update_cpu_nonexist(struct last_message_array* array,
    int cpu, u64 ts)
{
    struct last_message* message = &array->messages[cpu];
    BUG_ON(cpumask_test_cpu(cpu, &array->message_exist));
    message->ts = ts;
    //update ordering
    list_del(&message->list);
    last_message_list_add(&array->messages_ordered,
        message);

}

static int
last_message_array_set_cpu_message(struct last_message_array* array,
    int cpu, u64 ts, const void* data, size_t size)
{
    struct last_message* message = &array->messages[cpu];
    BUG_ON(cpumask_test_cpu(cpu, &array->message_exist));

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
        data,
        size);
    message->size = size;
    cpumask_set_cpu(cpu, &array->message_exist);
    message->ts = ts;
    //update ordering
    list_del(&message->list);
    last_message_list_add(&array->messages_ordered,
        message);

    //pr_info("Message on cpu %d with timestamp %lu.",
    //    cpu, (unsigned long)message->ts);
    return 0;
}

static void
last_message_array_clear_cpu_message(struct last_message_array* array,
    int cpu)
{
    cpumask_clear_cpu(cpu, &array->message_exist);
    // Do not free old message contents - for realloc
}

static void
last_message_array_destroy(struct last_message_array* array)
{
    int cpu;
    for_each_possible_cpu(cpu)
    {
        kfree(array->messages[cpu].msg);
    }
}

/* 
 * Format of tracing data
 */
struct trace_data
{
    u64 ts;
    char data[0];
};

/*
 * Struct, represented buffer which support two main operations:
 *
 * 1.Writing message into buffer
 * 2.Extract oldest message from the buffer.
 *   If buffer is empty, may wait.
 */

struct trace_buffer
{
    struct ring_buffer* buffer;

    /*
     * Array of last messages from all possible CPUs
     */
    struct last_message_array last_message_array;
    
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
    // Clock-related variables and functions
    u64 (*clock)(void);
    u64 clock_delta;
};

static void wake_up_reader(struct work_struct *work)
{
    struct trace_buffer* trace_buffer = container_of(to_delayed_work(work), struct trace_buffer, work_wakeup_reader);
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
    for_each_possible_cpu(cpu)
    {
        last_message_array_clear_cpu_message(&trace_buffer->last_message_array,
            cpu);
    }

    trace_buffer->messages_lost_internal = 0;
    ring_buffer_reset(trace_buffer->buffer);
}

/*
 * Allocate buffer.
 * 
 * 'size' is size of the buffer created.
 * 'mode_overwrite' determine policy,
 *  when size is overflowed while writing message:
 *   if 'mode_overwrite' is 0, then newest message will be dropped.
 *   otherwise the oldest message will be dropped.
 */
struct trace_buffer* trace_buffer_alloc(
    size_t size, bool mode_overwrite)
{
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
    last_message_array_init(&trace_buffer->last_message_array, 0);
    
    mutex_init(&trace_buffer->read_mutex);
    
    trace_buffer->messages_lost_internal = 0;
    
    init_waitqueue_head(&trace_buffer->rq);
    
    INIT_DELAYED_WORK(&trace_buffer->work_wakeup_reader, wake_up_reader);
    //setup clock
    trace_buffer->clock = kedr_clock;
    trace_buffer->clock_delta = KEDR_CLOCK_DELTA;
    
    return trace_buffer;
}
/*
 * Destroy buffer, free all resources which it used.
 */
void trace_buffer_destroy(struct trace_buffer* trace_buffer)
{
    cancel_delayed_work_sync(&trace_buffer->work_wakeup_reader);
    
    mutex_destroy(&trace_buffer->read_mutex);

    last_message_array_destroy(&trace_buffer->last_message_array);
    
    ring_buffer_free(trace_buffer->buffer);
    kfree(trace_buffer);
}

/*
 * Reserve space in the buffer for writing message.
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
    struct trace_data* msg_real;
    struct ring_buffer_event* event =
        ring_buffer_lock_reserve(trace_buffer->buffer,
            sizeof(*msg_real) + size);
    if(event == NULL) return NULL;
    msg_real = ring_buffer_event_data(event);
    *msg = (void*)msg_real->data;
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
    struct trace_data *msg_real = ring_buffer_event_data(event);
    msg_real->ts = trace_buffer->clock();
    ring_buffer_unlock_commit(trace_buffer->buffer, event);
}

/*
 * Write message with data 'msg' of length 'size' to the buffer.
 * May be called in the atomic context.
 */
void trace_buffer_write_message(struct trace_buffer* trace_buffer,
    const void* msg, size_t size)
{
    void* data;
    void* id = trace_buffer_write_lock(trace_buffer, size, &data);
    if(id == NULL) return;

    memcpy(data, msg, size);

    trace_buffer_write_unlock(trace_buffer, id);
}


/*
 * Non-blocking read of only current oldest message(without reading of the per-cpu buffers).
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
    struct last_message_array* last_message_array =
        &trace_buffer->last_message_array;
    int oldest_cpu;
    struct last_message* oldest_message =
        list_first_entry(&last_message_array->messages_ordered,
                            struct last_message, list);
    oldest_cpu = oldest_message - last_message_array->messages;

    if(!cpumask_test_cpu(oldest_cpu, &last_message_array->message_exist))
    {
        return -EAGAIN;
    }

    result = process_data(oldest_message->msg,
        oldest_message->size,
        oldest_cpu,
        oldest_message->ts,
        &consume,
        user_data);
    //Remove oldest message if it is consumed
    if(consume)
    {
        last_message_array_clear_cpu_message(last_message_array,
            oldest_cpu);
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
    struct last_message_array* last_message_array = 
        &trace_buffer->last_message_array;

    if(wait_function)
        wait_function(&trace_buffer->rq, data);
    
    cpumask_clear(&subbuffers_updated);
    // Try to determine oldest message in the buffer(from all cpu's)
    for(oldest_message = list_first_entry(&last_message_array->messages_ordered, struct last_message, list);
        !cpumask_test_cpu(oldest_message - last_message_array->messages, &last_message_array->message_exist);
        oldest_message = list_first_entry(&last_message_array->messages_ordered, struct last_message, list))
    {
        // Cannot determine latest message - need to update timestamp
        int cpu = oldest_message - last_message_array->messages;
        u64 ts;
        u64 empty_ts;
        struct ring_buffer_event* event;
        
        if(cpumask_test_cpu(cpu, &subbuffers_updated))
        {
            //This cpu-buffer has already been tested. Buffer is cannot read now.
            if(wait_function)
                schedule_delayed_work(&trace_buffer->work_wakeup_reader,
                    (cpumask_empty(&last_message_array->message_exist) ? TIME_WAIT_BUFFER : TIME_WAIT_SUBBUFFER)
                    * HZ / 1000/*jiffies in ms*/);
            
            return -EAGAIN;
        }
        cpumask_set_cpu(cpu, &subbuffers_updated);
        
        empty_ts = trace_buffer->clock() - trace_buffer->clock_delta;
#if defined(RING_BUFFER_CONSUME_HAS_4_ARGS)
		event = ring_buffer_consume(trace_buffer->buffer, cpu, &ts, NULL);
#elif defined(RING_BUFFER_CONSUME_HAS_3_ARGS)
		event = ring_buffer_consume(trace_buffer->buffer, cpu, &ts);
#else
#error RING_BUFFER_CONSUME_HAS_4_ARGS or RING_BUFFER_CONSUME_HAS_3_ARGS should be defined.
#endif

        if(event)
        {
            size_t size;
            struct trace_data* msg = ring_buffer_event_data(event);
            size = ring_buffer_event_length(event) - sizeof(*msg);
            if(last_message_array_set_cpu_message(last_message_array,
                cpu, msg->ts, msg->data, size))
            {
                pr_err("Cannot allocate new message.");
                trace_buffer->messages_lost_internal++;
                return -ENOMEM;
            }
        }
        else
        {
            last_message_array_update_cpu_nonexist(last_message_array,
                cpu, empty_ts);
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
 * If error occurs, return negative error code.
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

    while(((result = trace_buffer_update_internal(trace_buffer, NULL, NULL)) == -EAGAIN)
        && should_wait)
    {
        //perform wait_event_killable()
        struct read_wait_table table;
        read_wait_init(&table);
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
