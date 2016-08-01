/*
 * Implementation of the 'trace_buffer' API.
 */
#include "trace_buffer.h"
#include "trace_config.h"

#include <linux/ring_buffer.h> /* ring buffer functions*/
#include <linux/threads.h> /*definition of NR_CPUS macro*/
#include <linux/mutex.h> /* mutexes */
#include <linux/slab.h> /* kmalloc and others*/
#include <linux/wait.h> /*wait queue definitions*/
#include <linux/sched.h> /* TASK_NORMAL, TASK_INTERRUPTIBLE*/
#include <linux/hardirq.h> /* in_nmi() */
#include <linux/hrtimer.h> /* high resolution timer for clock*/

#include "config.h"

/*
 * Standard clock for ring buffer cannot be used interprocessor message
 * ordering. It is not a problem, when messages are output per-cpu,
 * but it is critical when we want combine all of them into single trace.
 * 
 * So, we need to implement our clock.
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
 * Format of ring buffer trace data.
 * 
 * 'struct ring_buffer' actually has a field for clock function,
 * but function which set clock is not exported for kernel modules.
 * 
 * So we store timestamps alongside with message data.
 */
struct trace_data
{
	u64 ts;
	char msg[0];
};

#define msg_to_event_size(msg_size) ((msg_size) + offsetof(struct trace_data, msg))
#define event_to_msg_size(event_size) ((event_size) - offsetof(struct trace_data, msg))

/*
 * Last message extracted from per-cpu buffer.
 */
struct last_message
{
	/* List of per-cpu last messages ordered by timestamp. */
	struct list_head list;
	/* 
	 * Event extracted or NULL if buffer found to be empty.
	 * */
	struct ring_buffer_event* event;
	
	/*
	 * Timestamp of the message.
	 * 
	 * If no message is stored, this is timestamp
	 * which cannot be more than any possible timestamp of the future message.
	 */
	u64 ts;
};

void last_message_set(struct last_message* lm,
	struct ring_buffer_event* event)
{
	struct trace_data* td;
	
	BUG_ON(lm->event);
	
	td = ring_buffer_event_data(event);
	lm->ts = td->ts;
	lm->event = event;
}

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

	//Array of 'last_message' content for corresponding CPUs.
	struct last_message* last_messages;
	//List organization of 'last_messages', ordered by .ts in ascended order.
	struct list_head last_messages_ordered;
	
	/* 
	 * Prevent concurrent access to 'last_message' array
	 * and 'last_messages_ordered' list.
	 */
	struct mutex m;
	
	// Wait queue for polling
	wait_queue_head_t rq;

	u64 (*clock)(void);
	
	/*
     *  Pointer to the first callback(for call it when needed)
     * and to the last callback for add new ones.
     */
    struct kedr_trace_callback_head* callback_first;
    struct kedr_trace_callback_head** callback_last_p;

	/*
	 * Protect callbacks list and their execution.
	 */
	spinlock_t cb_lock;
};

void trace_buffer_clear_last_message(struct trace_buffer* tb, int cpu)
{
	if(tb->last_messages[cpu].event)
	{
		tb->last_messages[cpu].event = NULL;
		ring_buffer_consume_compat(tb->buffer, cpu, NULL);
	}
}

/* 
 * Execute all callbacks with timestamp less than given one.
 * 
 * Should be executed under cb_lock.
 */
static inline void
execute_callbacks_before_ts(struct trace_buffer* tb, u64 ts)
{
	while(tb->callback_first)
	{
		struct kedr_trace_callback_head* callback = tb->callback_first;
		if(callback->ts >= ts) break;

		tb->callback_first = callback->next;
		if(!tb->callback_first)
		{
			tb->callback_last_p = &tb->callback_first;
		}
		callback->func(callback);
	}
}

/* 
 * Execute all callbacks.
 * 
 * Should be executed under cb_lock or without concurrency.
 */
static inline void
execute_callbacks_all(struct trace_buffer* tb)
{
	while(tb->callback_first)
	{
		struct kedr_trace_callback_head* callback = tb->callback_first;

		tb->callback_first = callback->next;
		if(!tb->callback_first)
		{
			tb->callback_last_p = &tb->callback_first;
		}
		callback->func(callback);
	}
}


/*
 * Clear all messages in the buffer.
 *
 * Should be executed with lock taken.
 */

static void trace_buffer_clear_internal(struct trace_buffer* tb)
{
	unsigned long flags;
	int cpu;
	//Clear last messages
	for_each_possible_cpu(cpu)
	{
		trace_buffer_clear_last_message(tb, cpu);
	}

	spin_lock_irqsave(&tb->cb_lock, flags);
	ring_buffer_reset(tb->buffer);
	execute_callbacks_all(tb);
	spin_unlock_irqrestore(&tb->cb_lock, flags);
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
struct trace_buffer* trace_buffer_alloc(size_t size, bool mode_overwrite)
{
	int cpu;
	u64 ts;
	struct trace_buffer* tb = kmalloc(sizeof(*tb), GFP_KERNEL);
	
	if(tb == NULL)
	{
		pr_err("%s: Cannot allocate trace_buffer structure.", __func__);
		return NULL;
	}
	
	tb->buffer = ring_buffer_alloc(size, mode_overwrite? RB_FL_OVERWRITE : 0);
	if(tb->buffer == NULL)
	{
		pr_err("%s: Cannot allocate ring buffer.", __func__);
		kfree(tb);
		return NULL;
	}
	
	//setup clock
	tb->clock = kedr_clock;
	ts = tb->clock();

	tb->last_messages = kmalloc(num_possible_cpus() * sizeof(struct last_message), GFP_KERNEL);
	if(tb->last_messages == NULL)
	{
		pr_err("%s: Cannot allocate array of last messages.", __func__);
		ring_buffer_free(tb->buffer);
		kfree(tb);
		return NULL;
	}

	INIT_LIST_HEAD(&tb->last_messages_ordered);
	for_each_possible_cpu(cpu)
	{
		struct last_message* lm = &tb->last_messages[cpu];

		lm->event = NULL;
		lm->ts = ts;
		list_add_tail(&lm->list, &tb->last_messages_ordered);
	}

	mutex_init(&tb->m);
	spin_lock_init(&tb->cb_lock);

	init_waitqueue_head(&tb->rq);

	tb->callback_first = NULL;
	tb->callback_last_p = &tb->callback_first;
	
	return tb;
}
/*
 * Destroy buffer, free all resources which it used.
 */
void trace_buffer_destroy(struct trace_buffer* tb)
{
	execute_callbacks_all(tb);
	
	mutex_destroy(&tb->m);

	ring_buffer_free(tb->buffer);
	kfree(tb->last_messages);
	kfree(tb);
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
void* trace_buffer_write_lock(struct trace_buffer* tb,
	size_t size, void** msg)
{
	struct trace_data* msg_real;
	struct ring_buffer_event* event =
		ring_buffer_lock_reserve(tb->buffer, msg_to_event_size(size));
	if(event == NULL) return NULL;
	msg_real = ring_buffer_event_data(event);
	*msg = (void*)msg_real->msg;
	return event;
}

/*
 * Commit message written after previous call
 * trace_buffer_write_lock().
 * 
 * May be called in the atomic context.
 */
void trace_buffer_write_unlock(struct trace_buffer* tb,
	void* id)
{
	struct ring_buffer_event* event = (struct ring_buffer_event*)id;
	struct trace_data *msg_real = ring_buffer_event_data(event);
	msg_real->ts = tb->clock();
	ring_buffer_unlock_commit(tb->buffer, event);
	/* It is sufficient to check waitqueue emptiness without lock */
	if(waitqueue_active(&tb->rq))
	{
		wake_up_all(&tb->rq);
	}
}

/*
 * Write message with data 'msg' of length 'size' to the buffer.
 * May be called in the atomic context.
 */
void trace_buffer_write_message(struct trace_buffer* tb,
	const void* msg, size_t size)
{
	void* data;
	void* id = trace_buffer_write_lock(tb, size, &data);
	if(id == NULL) return;

	memcpy(data, msg, size);

	trace_buffer_write_unlock(tb, id);
}


/*
 * Update the oldest message if needed.
 *
 * Should be executed under mutex locked.
 *
 * Return 0 on success. On fail return negative error code.
 * If buffer is empty, return -EAGAIN.
 * 
 * Main principles of the implementation:
 * 1. syncronize_sched() is used only when it is neccessary.
 * 2. Buffer may be treated as empty only if for every per-cpu buffer
 * its the last check reveales it is empty.
 */

static int trace_buffer_update_internal(struct trace_buffer* tb)
{
	unsigned long flags;
	/* Timestamp for empty per-cpu buffers. */
	u64 ts_empty;
	/* Whether ts_empty is set. */
	bool ts_empty_set = 0;
	/* Whether non-empty per-cpu buffer is found. */
	bool non_empty_buffer_found = 0;
	
	struct last_message* oldest_message;
	
	while(1)
	{
		struct ring_buffer_event* event;
		struct last_message* lm;
		int cpu;
		
		
		oldest_message = list_first_entry(&tb->last_messages_ordered,
			struct last_message, list);
		
		if(oldest_message->event) break; // Oldest message is already set.
		
		cpu = oldest_message - tb->last_messages;
		
		/* 
		 * (Re)check messages in the cpu corresponded to the empty last
		 * message.
		 * 
		 * Iterations are finite, see comments below.
		 */
		
		event = ring_buffer_peek_compat(tb->buffer, cpu, NULL);
		
		if(event)
		{
			last_message_set(oldest_message, event);
			
			non_empty_buffer_found = 1;
			
			// Reorder given last message, if needed
			lm = oldest_message;
			list_for_each_entry_continue(lm, &tb->last_messages_ordered, list)
			{
				if(lm->ts >= oldest_message->ts) break;
			}
			
			if(oldest_message->list.next != &lm->list)
			{
				// Reorder
				list_move_tail(&oldest_message->list, &lm->list);
			}
			
			/* 
			 * Per-cpu buffer has message extracted, so it won't be
			 * checked again.
			 * 
			 * So, next iteration will check another per-cpu buffer.
			 */
			continue;
		}

		/* 
		 * The oldest message is empty.
		 * 
		 * Assign new timestamp for it.
		 */
		if(!ts_empty_set)
		{
			ts_empty = tb->clock();
			/* 
			 * All messages which is read previously has timestamp less
			 * than given one.
			 */
			
			synchronize_sched();
			/* 
			 * Since now, if we found empty cpu-buffer, ts for its future
			 * messages cannot be less than 'ts_empty'.
			 */
			ts_empty_set = 1;
			
			/* Now we need to re-read message from current per-cpu buffer. */
			continue;
		}
		
		oldest_message->ts = ts_empty;
				
		// Reorder given last message, if needed
		lm = oldest_message;
		list_for_each_entry_continue(lm, &tb->last_messages_ordered, list)
		{
			if(lm->ts >= oldest_message->ts) break;
		}
		
		if(oldest_message->list.next != &lm->list)
		{
			// Reorder
			list_move_tail(&oldest_message->list, &lm->list);
			
			/*
			 * Every per-cpu buffer which is found to be empty has
			 * 'ts_empty' timestamp.
			 * 
			 * As next iterations checks only per-cpu buffer of
			 * 'oldest_message', which has timestamp less than 'ts_empty'
			 * current per-cpu buffer won't be checked again.
			 */
			 continue;
		}

		/* 
		 * The oldest message has 'ts_empty' timestamp.
		 * 
		 * Because 'ts_empty' is greater than any timestamp obtained
		 * before this function is called, this is mean that
		 * every per-cpu buffer is checked.
		 */
		if(!non_empty_buffer_found)
		{
			/* Every per-cpu buffer is checked and found to be empty. */
			spin_lock_irqsave(&tb->cb_lock, flags);
			execute_callbacks_before_ts(tb, ts_empty);
			spin_unlock_irqrestore(&tb->cb_lock, flags);
			return -EAGAIN;
		}
			
		/*
		 * 'ts_empty' is less than timestamp of the non-empty message.
		 * 
		 * Pick up new value for 'ts_empty' for make this ordering
		 * impossible, and continue iterations.
		 * 
		 * This will re-read current per-cpu buffer and, possibly, some
		 * other empty ones. But we never be here again.
		 */
		ts_empty = tb->clock();
		synchronize_sched();
	}
   
	spin_lock_irqsave(&tb->cb_lock, flags);
	execute_callbacks_before_ts(tb, oldest_message->ts);
	spin_unlock_irqrestore(&tb->cb_lock, flags);

	return 0;
}

int
trace_buffer_read(struct trace_buffer* tb,
	int (*process_msg)(const void* msg, size_t size, int cpu,
		u64 ts, void* user_data),
	void* user_data)
{
	int err = 0;

	int cpu;
	struct trace_data* td;
	struct last_message* oldest_message;

	if(mutex_lock_killable(&tb->m))
		return -ERESTARTSYS;


	err = trace_buffer_update_internal(tb);
	if(err) goto out;

	oldest_message = list_first_entry(&tb->last_messages_ordered, struct last_message, list);
	
	BUG_ON(!oldest_message->event);
	
	cpu = oldest_message - tb->last_messages;
	td = ring_buffer_event_data(oldest_message->event);
	
	err = process_msg(td->msg,
		event_to_msg_size(ring_buffer_event_length(oldest_message->event)),
		cpu,
		oldest_message->ts,
		user_data);
	
	if(err > 0)
	{
		trace_buffer_clear_last_message(tb, cpu);
	}
	
out:
	mutex_unlock(&tb->m);
	return err;
}


wait_queue_head_t*
trace_buffer_get_wait_queue(struct trace_buffer* tb)
{
	return &tb->rq;
}

/*
 * Return number of messages lost due to the buffer overflow.
 */
unsigned long
trace_buffer_lost_messages(struct trace_buffer* tb)
{
	return ring_buffer_overruns(tb->buffer);
}

/*
 * Reset trace in the buffer.
 * 
 * Return 0 on success, negative error code otherwise.
 */
int
trace_buffer_reset(struct trace_buffer* tb)
{
	if(mutex_lock_interruptible(&tb->m))
	{
		return -ERESTARTSYS;
	}
	trace_buffer_clear_internal(tb);
	mutex_unlock(&tb->m);
	return 0;
}

/*
 * Return size of buffer in bytes.
 */
unsigned long
trace_buffer_size(struct trace_buffer* tb)
{
	return ring_buffer_size_compat(tb->buffer);
}

/*
 * Change size of the buffer.
 *
 * Current messages in the buffer may be silently lost.
 * (in the current implementation, the buffer is forcibly reset).
 *
 * Return new size on success, negative error code otherwise.
 */

int trace_buffer_resize(struct trace_buffer* tb,
	unsigned long size)
{
	int result;
	if(mutex_lock_interruptible(&tb->m))
	{
		return -ERESTARTSYS;
	}
	
	result = ring_buffer_resize_compat(tb->buffer, size);
	
	if(!result)
	{
		trace_buffer_clear_internal(tb);
	}
	
	mutex_unlock(&tb->m);
	return result;
}

void trace_buffer_call_after_read(struct trace_buffer* tb,
    kedr_trace_callback_func func,
    struct kedr_trace_callback_head* callback_head)
{
	unsigned long flags;
	spin_lock_irqsave(&tb->cb_lock, flags);
	
	callback_head->func = func;
	callback_head->ts = tb->clock();
	
	callback_head->next = NULL;
	*tb->callback_last_p = callback_head;
	tb->callback_last_p = &callback_head->next;

	/* 
	 * Fast check whether buffer is currently empty.
	 */
	if(ring_buffer_empty(tb->buffer))
	{
		execute_callbacks_all(tb);
	}

	spin_unlock_irqrestore(&tb->cb_lock, flags);
}

u64 trace_buffer_clock(struct trace_buffer* tb)
{
	return tb->clock();
}
