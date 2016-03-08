#ifndef MY_TRACE_BUFFER_H
#define MY_TRACE_BUFFER_H

#include <linux/types.h>
#include <linux/poll.h>

#include <kedr/trace/trace.h>

/*
 * Struct, represented buffer which support two main operations:
 *
 * 1.Writing message into buffer
 * 2.Extract the oldest message from the buffer.
 *   If buffer is empty, may wait.
 */

struct trace_buffer;

/*
 * Allocate buffer.
 * 
 * 'size' is size of the buffer created.
 * 'mode_overwrite' determine policy,
 *  when size is overflowed while writing message:
 *   if 'mode_overwrite' is 0, then newest message will be dropped.
 *   otherwise the oldest message will be dropped.
 */
struct trace_buffer*
trace_buffer_alloc(size_t size, bool mode_overwrite);
/*
 * Destroy buffer, free all resources which it used.
 */
void trace_buffer_destroy(struct trace_buffer* tb);

/*
 * Write message with data 'msg' of length 'size' to the buffer.
 * May be called in the atomic context.
 */
void trace_buffer_write_message(struct trace_buffer* tb,
    const void* msg, size_t size);

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
    size_t size, void** msg);

/*
 * Commit message written after previous call
 * trace_buffer_write_lock().
 * 
 * May be called in the atomic context.
 */
void trace_buffer_write_unlock(struct trace_buffer* tb,
    void* id);

/*
 * Read the oldest message from the buffer.
 * 
 * For message extracted call 'process_msg':
 * 'msg' is set to the pointer to the message data.
 * 'size' is set to the size of the message,
 * 'ts' is set to the timestamp of the message,
 * 'user_data' is set to the 'user_data' parameter of the function.
 * 
 * If 'process_msg' return positive value, message is assumed to be consumed,
 * so future reading will extract next message.
 * Return value, returned by 'process_msg', or -EAGAIN if buffer is empty.
 * 
 * Shouldn't be called in atomic context.
 */
int
trace_buffer_read(struct trace_buffer* tb,
    int (*process_msg)(const void* msg, size_t size, int cpu,
        u64 ts, void* user_data),
    void* user_data);

/*
 * Return waitqueue for wait, when buffer become non-empty.
 * 
 * Usage:
 * 
 * 0. wq = trace_buffer_get_wait_queue(tb)
 * 1. add_wait_queue(wq, wait)
 * 2. trace_buffer_read(tb, ...)
 * 3. If previos step reveals buffer emptiness, wait using 'wait'.
 * 
 * Note, that standard wait_event() template, where set_task_state()
 * is executed before checking of event, is not suitable for trace_buffer.
 * The thing is that event checking (trace_buffer_read() call) uses
 * schedule functions inside(e.g., mutex locking), which incorrectly
 * interfers with modified task state.
 */
wait_queue_head_t*
trace_buffer_get_wait_queue(struct trace_buffer* tb);


/*
 * Return number of messages lost due to the buffer overflow.
 */
unsigned long
trace_buffer_lost_messages(struct trace_buffer* tb);

/*
 * Reset trace in the buffer.
 * 
 * Return 0 on success, negative error code otherwise.
 */
int
trace_buffer_reset(struct trace_buffer* tb);

/*
 * Return size of buffer in bytes.
 */
unsigned long
trace_buffer_size(struct trace_buffer* tb);

/*
 * Change size of the buffer.
 *
 * Current messages in the buffer may be silently lost.
 * (in current implementation buffer is forcibly reseted).
 *
 * Return new size on success, negative error code otherwise.
 */

int
trace_buffer_resize(struct trace_buffer* tb, unsigned long size);


/* 
 * Call 'func' after all messages, written into buffer until this moment,
 * are consumed(via read or other ways).
 */
void trace_buffer_call_after_read(struct trace_buffer* tb,
    kedr_trace_callback_func func,
    struct kedr_trace_callback_head* callback_head);

u64 trace_buffer_clock(struct trace_buffer* tb);

#endif /* MY_TRACE_BUFFER_H */
