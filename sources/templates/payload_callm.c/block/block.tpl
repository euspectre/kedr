//***** Monitor call of <$function.name$> *****//

//Implementation of tracing point
<$if concat(trace.param.name)$>
struct kedr_trace_fc_data_<$function.name$>
{
    <$entryField : join(\n\t)$>
};
<$endif$>

<$if trace.formatString$>static inline int
kedr_trace_fc_pp_function_<$function.name$>(char* dest, size_t size,
	const void* data)
{
<$if concat(trace.param.name)$>	struct kedr_trace_fc_data_<$function.name$>* __entry =
		(struct kedr_trace_fc_data_<$function.name$>*)data;

	return snprintf(dest, size, <$trace.formatString$>,
		<$entryItem : join(,\n\t\t)$>);
<$else$>
	return snprintf(dest, size, <$trace.formatString$>);
<$endif$>}
<$endif$>

// Interception function itself
#define KEDR_<$if trace.happensBefore$>PRE<$else$>POST<$endif$>_<$function.name$>
static void
kedr_<$if trace.happensBefore$>pre<$else$>post<$endif$>_<$function.name$>(<$argumentSpec_comma$>
	<$if trace.happensBefore$><$else$><$if returnType$><$returnType$> ret_val,
	<$endif$><$endif$>struct kedr_function_call_info* call_info)
{
<$if concat(trace.param.name)$>	struct kedr_trace_fc_data_<$function.name$> __entry;
<$endif$><$if concat(prologue)$><$prologue: join(\n)$>

<$endif$><$if concat(trace.param.name)$>	<$entryAssign : join(\n\t\t)$>
<$endif$>	kedr_trace_function_call("<$function.name$>",
	call_info->return_address,
	<$if trace.formatString$>&kedr_trace_fc_pp_function_<$function.name$><$else$>NULL<$endif$>,
	<$if concat(trace.param.name)$>&__entry, sizeof(__entry)<$else$>NULL, 0<$endif$>);
<$if concat(epilogue)$>
<$epilogue: join(\n)$>
<$endif$>}
