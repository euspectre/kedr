#ifndef KEDR_TRACE_H
#define KEDR_TRACE_H

/*
 * Pretty print for data in the trace.
 * 
 * This function should behave as snprintf(dest, size, ...):
 * 
 * -Write no more than 'size' characters into 'dest'.
 * -Always append '\0' byte to the output(if size>=1)
 * -Return number of characters(exclude '\0'),
 *   which will be written in case, when 'dest' have enough space.
 */
typedef int (*kedr_trace_pp_function)(char* dest, size_t size, const void* data);

void kedr_trace_pp_unregister(void);

/*
 * Add message into the trace.
 * 
 * 'data' and 'size' determine message to be saved in the trace.
 * 
 * 'pp' is pretty print function which will be called when given message
 * will be read via trace file.
 */
void kedr_trace(kedr_trace_pp_function pp,
	const void* data, size_t size);

/*
 * Add message about function's call into the trace.
 * 
 * If not NULL, 'params_pp' is used for print additional information
 * about call(e.g., function's parameters).
 */
void kedr_trace_function_call(const char* function_name,
	void* return_address, kedr_trace_pp_function params_pp,
	const void* params, size_t params_size);

/*
 * Reserve space for message in the trace.
 * 
 * Return not NULL on success. Returning value should be passed to
 * the kedr_trace_unlock_commit() for complete trace operation.
 */

void* kedr_trace_lock(kedr_trace_pp_function pp,
	size_t size, void** data);

/*
 * Reserve space for function call message in the trace.
 * 
 * Return not NULL on success. Returning value should be passed to
 * the kedr_trace_unlock_commit() for complete trace operation.
 */
void* kedr_trace_function_call_lock(const char* function_name,
	void* return_address, kedr_trace_pp_function params_pp,
	size_t params_size,	void** params);


/*
 * Complete trace operation, started with kedr_trace_lock() or
 * kedr_trace_function_call_lock().
 */
void kedr_trace_unlock_commit(void* id);


struct kedr_trace_callback_head;

typedef void (*kedr_trace_callback_func)(struct kedr_trace_callback_head*);

struct kedr_trace_callback_head
{
	struct kedr_trace_callback_head* next;
	kedr_trace_callback_func func;
	u64 ts;
};

/*
 * Call given function after all trace messages, commited up to this
 * moment, will be read or flushed in some way.
 * 
 * Function is garanteed to be called on buffer flushing or destroying
 * if corresponded kedr_trace_call_after_read call happens before
 * corresponded operation.
 * 
 * Callback function is executed in the atomic context.
 * 
 * Currently, kedr_trace_call_after_read may wait.
 */
void kedr_trace_call_after_read(kedr_trace_callback_func func,
	struct kedr_trace_callback_head* callback_head);

#endif /* KEDR_TRACE_H */
