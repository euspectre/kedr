#ifndef MY_TRACE_BUFFER_H
#define MY_TRACE_BUFFER_H

#include <linux/types.h>

#include <linux/wait.h>
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
void trace_buffer_destroy(struct trace_buffer* trace_buffer);

/*
 * Write message with data 'msg' of length 'size' to the buffer.
 * May be called in the atomic context.
 */
void trace_buffer_write_message(struct trace_buffer* trace_buffer,
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
void* trace_buffer_write_lock(struct trace_buffer* trace_buffer,
    size_t size, void** msg);

/*
 * Commit message written after previous call
 * trace_buffer_write_lock().
 * 
 * May be called in the atomic context.
 */
void trace_buffer_write_unlock(struct trace_buffer* trace_buffer,
    void* id);

/*
 * Read the oldest message from the buffer.
 * 
 * For message read call 'process_data':
 * 'msg' is set to the pointer to the message data.
 * 'size' is set to the size of the message,
 * 'ts' is set to the timestamp of the message,
 * 'user_data' is set to the 'user_data' parameter of the function.
 * 
 * Return value, which is returned by 'process_data'.
 *
 * If 'process_data' set 'consume' parameter to not 0,
 * message is treated consumed, and next reading return next message from buffer.
 * Otherwise, next reading return the same message.
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
    void* user_data);


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
    void* data);


/*
 * Return number of messages lost due to the buffer overflow.
 */
unsigned long
trace_buffer_lost_messages(struct trace_buffer* trace_buffer);

/*
 * Reset trace in the buffer.
 * 
 * Return 0 on success, negative error code otherwise.
 */
int
trace_buffer_reset(struct trace_buffer* trace_buffer);

/*
 * Return size of buffer in bytes.
 */
unsigned long
trace_buffer_size(struct trace_buffer* trace_buffer);

/*
 * Change size of the buffer.
 *
 * Current messages in the buffer may be silently lost.
 * (in current implementation buffer is forcibly reseted).
 *
 * Return new size on success, negative error code otherwise.
 */

int
trace_buffer_resize(struct trace_buffer* trace_buffer,
    unsigned long size);

#endif