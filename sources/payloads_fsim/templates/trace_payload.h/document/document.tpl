/*********************************************************************
 * trace_payload.h: trace-related stuff
 *
 * Based on trace events sample by Steven Rostedt (kernel 2.6.33.1).
 *********************************************************************/

#undef TRACE_SYSTEM
#define TRACE_SYSTEM <$module.name$>

#if !defined(_TRACE_PAYLOAD_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PAYLOAD_H

/*********************************************************************
 * Definitions of trace events
 *********************************************************************/
<$block : join(\n\n)$>
/*********************************************************************/
#endif /* _TRACE_PAYLOAD_H */

/* This is for the trace events machinery to be defined properly */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .

#define TRACE_INCLUDE_FILE trace_payload
#include <trace/define_trace.h>
