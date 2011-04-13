//***** Monitor call of <$function.name$> *****//

//Implementation of tracing point
struct kedr_trace_data_<$function.name$>
{
	void* abs_addr;
	int section_id;
	ptrdiff_t rel_addr;<$if concat(trace.param.name)$>
	<$entryField : join(\n\t)$><$endif$>
};

static inline int
kedr_trace_pp_function_<$function.name$>(char* dest, size_t size,
	const void* data)
{
	struct kedr_trace_data_<$function.name$>* __entry =
		(struct kedr_trace_data_<$function.name$>*)data;

	return 	snprintf(dest, size,
		"%s: ([<%p>] %s+0x%tx)"<$if trace.formatString$>" "<$trace.formatString$><$endif$>,
		"called_<$function.name$>",
		__entry->abs_addr,
		kedr_print_symbolic(__entry->section_id,
			{module_section_unknown, 	"unknown"},
			{module_section_init,		"init"},
			{module_section_core, 		"core"}
		),
		__entry->rel_addr<$if concat(trace.param.name)$>,
		<$entryItem : join(,\n\t\t)$><$endif$>
	);
}

static inline void
trace_called_<$function.name$>(void* abs_addr,
	int section_id,
	ptrdiff_t rel_addr<$if concat(trace.param.name)$>,
	<$traceArgument : join(, )$><$endif$>)
{
	struct kedr_trace_data_<$function.name$> __entry;
	
	__entry.abs_addr = abs_addr;
	__entry.section_id = section_id;
	__entry.rel_addr = rel_addr;<$if concat(trace.param.name)$>
	<$entryAssign : join(\n\t\t)$><$endif$>

	kedr_trace(kedr_trace_pp_function_<$function.name$>,
		&__entry, sizeof(__entry));
}
// Interception function itself
#define KEDR_<$if trace.happensBefore$>PRE<$else$>POST<$endif$>_<$function.name$>
static void
kedr_<$if trace.happensBefore$>pre<$else$>post<$endif$>_<$function.name$>(<$if concat(arg.name)$><$argument : join(, )$>,
	<$endif$><$if trace.happensBefore$><$else$><$if returnType$><$returnType$> returnValue,
	<$endif$><$endif$>struct kedr_function_call_info* call_info)
{
	void *abs_addr = call_info->return_address; 
	int section_id;
	ptrdiff_t rel_addr;

<$if concat(prologue)$><$prologue: join(\n)$>

<$endif$>process_caller_address(abs_addr, &section_id, &rel_addr);

	trace_called_<$function.name$>(abs_addr, section_id, rel_addr<$if concat(trace.param.name)$>,
		<$traceParamCast : join(, )$><$endif$>);
<$if concat(epilogue)$>
<$epilogue: join(\n)$>
<$endif$>}