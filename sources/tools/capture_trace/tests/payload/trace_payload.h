#undef TRACE_SYSTEM
#define TRACE_SYSTEM kedr_payload

#if !defined(TRACE_TARGET_H) || defined(TRACE_HEADER_MULTI_READ)
#define TRACE_TARGET_H

/*********************************************************************
 * Definitions of trace events
 *********************************************************************/
TRACE_EVENT(kedr_payload,

	TP_PROTO(int some_arg),

	TP_ARGS(some_arg),

	TP_STRUCT__entry(
		__field(int, some_arg)
	),

	TP_fast_assign(
		__entry->some_arg = some_arg;
	),

	TP_printk("Capture me payload %d" ,__entry->some_arg)
);

/*********************************************************************/
#endif /* TRACE_TARGET_H */

/* This is for the trace events machinery to be defined properly */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .

#define TRACE_INCLUDE_FILE trace_payload
#include <trace/define_trace.h>
