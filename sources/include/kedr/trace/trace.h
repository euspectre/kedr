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
 * 'pp' is pretty print function which will be used for this data.
 */
void kedr_trace(kedr_trace_pp_function pp,
    const void* data, size_t size);

/*
 * Reserve space for message in the trace.
 * 
 * 'pp' is pretty print function which will be used for this data.
 * 
 * Return not NULL on success. Returning value should be passed to
 * the kedr_trace_unlock_commit() for complete trace operation.
 */

void* kedr_trace_lock(kedr_trace_pp_function pp,
    size_t size, void** data);

/*
 * Complete trace operation, started with kedr_trace_lock().
 */

void kedr_trace_unlock_commit(void* id);

// Auxiliary function for printing

struct kedr_symbolic_pair
{
    int key;
    const char* value;
};
static inline const char* kedr_resolve_symbolic(int key,
    struct kedr_symbolic_pair* pairs, int n)
{
    static char buffer[10];
    int i;
    for(i = 0; i < n; i++)
        if(key == pairs[i].key) return pairs[i].value;
    snprintf(buffer, sizeof(buffer), "%d", key);
    return buffer;
}
// Usage: kedr_print_symbolic(key, {key1, value1}, ...)
#define kedr_print_symbolic(key, ...) \
    ({struct kedr_symbolic_pair pairs[] = {__VA_ARGS__}; \
    kedr_resolve_symbolic(key, pairs, ARRAY_SIZE(pairs));})

/*
 * Auxiliary functions for trace target session markers
 * (use only for KEDR).
 */

#include <linux/module.h>

void kedr_trace_marker_target(struct module* target_module,
    struct module* payload_module, bool is_begin);

#endif
