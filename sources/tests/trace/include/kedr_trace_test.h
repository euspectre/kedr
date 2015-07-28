#ifndef KEDR_TRACE_TEST_INCLUDED
#define KEDR_TRACE_TEST_INCLUDED

#include <kedr/trace/trace.h>

/* Add message like 'test_message_<str>' into trace. */
void kedr_trace_test_msg_len(const char* str, size_t len);
static inline void kedr_trace_test_msg(const char* str)
{
    kedr_trace_test_msg_len(str, strlen(str));
}

/* Add message about 'test_function' call with given string parameter into trace. */
void kedr_trace_test_call_msg_len(void* caller_address, const char* param, size_t len);
static inline void kedr_trace_test_call_msg(void* caller_address, const char* param)
{
    kedr_trace_test_call_msg_len(caller_address, param, strlen(param));
}



#endif /* KEDR_TRACE_TEST_INCLUDED */