/*********************************************************************
 * controller_tracing.h: trace-related stuff for kedr-controller
 *
 * Based on trace events sample by Steven Rostedt (kernel 2.6.33.1).
 *********************************************************************/

#undef TRACE_SYSTEM
#define TRACE_SYSTEM kedr_controller

#if !defined(_CONTROLLER_TRACING_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CONTROLLER_TRACING_H

#include <linux/tracepoint.h>

/* Size of the buffer used to output strings to the trace (in bytes) */
#ifndef KEDR_TRACE_BUFFER_SIZE
#  define KEDR_TRACE_BUFFER_SIZE 128
#endif

/*********************************************************************
 * Definitions of trace events
 *********************************************************************/
/* This event is triggered each time the controller has instrumented
 * a target module.
 */
TRACE_EVENT(target_session_begins,

	TP_PROTO(char* tname),

	TP_ARGS(tname),

	TP_STRUCT__entry(
		__array(char, tname, KEDR_TRACE_BUFFER_SIZE)
	),

	TP_fast_assign(
		strncpy(__entry->tname, tname, KEDR_TRACE_BUFFER_SIZE - 1);
		__entry->tname[KEDR_TRACE_BUFFER_SIZE - 1] = 0;
	),

	TP_printk("target module: \"%s\"",
		__entry->tname
	)
);

/* This event is triggered each time the controller has finished monitoring
 * the target module (usually right before the target module is unloaded).
 */
TRACE_EVENT(target_session_ends,

	TP_PROTO(char* tname),

	TP_ARGS(tname),

	TP_STRUCT__entry(
		__array(char, tname, KEDR_TRACE_BUFFER_SIZE)
	),

	TP_fast_assign(
		strncpy(__entry->tname, tname, KEDR_TRACE_BUFFER_SIZE - 1);
		__entry->tname[KEDR_TRACE_BUFFER_SIZE - 1] = 0;
	),

	TP_printk("target module: \"%s\"",
		__entry->tname
	)
);
/*********************************************************************/
#endif /* _CONTROLLER_TRACING_H */

/* This is for the trace events machinery to be defined properly */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .

#define TRACE_INCLUDE_FILE controller_tracing
#include <trace/define_trace.h>
