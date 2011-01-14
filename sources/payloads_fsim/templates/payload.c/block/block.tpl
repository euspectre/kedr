//***** Replacement for <$function.name$> *****//

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
		kedr_print_symbolic(__entry->section_id, {0, "unknown"}, {1, "init"}, {2, "core"}, {3, "not_supported"}),
		__entry->rel_addr<$if concat(trace.param.name)$>,
		<$entryItem : join(,\n\t\t)$><$endif$>
	);
}

static inline void
trace_called_<$function.name$>(void* abs_addr,
	int section_id,
	ptrdiff_t rel_addr<$if concat(trace.param.name)$>,
	<$param : join(,\n\t)$><$endif$>)
{
	struct kedr_trace_data_<$function.name$> __entry;
	
	__entry.abs_addr = abs_addr;
	__entry.section_id = section_id;
	__entry.rel_addr = rel_addr;<$if concat(trace.param.name)$>
	<$entryAssign : join(\n\t\t)$><$endif$>

	kedr_trace(kedr_trace_pp_function_<$function.name$>,
		&__entry, sizeof(__entry));
}
// Replacement function itself
<$if fpoint.fault_code$><$if fpoint.reuse_point$><$else$>
static struct kedr_simulation_point* fsim_point_<$function.name$>;
#define fsim_point_attributes_<$function.name$> {&fsim_point_<$function.name$>, "<$function.name$>", "<$fpoint.param.type : join(,)$>"}
<$if concat(fpoint.param.name)$>struct fsim_point_data_<$function.name$>
{
<$fsimDataMember : join(\n)$>
};
<$endif$><$endif$><$endif$>
static <$if returnType$><$returnType$><$else$>void<$endif$>
repl_<$function.name$>(<$argumentSpec$>)
{<$if returnType$>
	<$returnType$> returnValue;<$endif$>
	void *abs_addr; 
	ptrdiff_t rel_addr;
	int section_id;
<$if fpoint.fault_code$><$if concat(fpoint.param.name)$>
	struct fsim_point_data_<$pointFunctionName$> fsim_point_data;<$endif$><$endif$>

<$prologue$>
	// Determine caller address (absolute and relative in the section)
	get_caller_address(abs_addr, section_id, rel_addr);

<$if trace.happensBefore$><$if returnType$>#error "Happens before relationship cannot be used for functions which return values."
<$endif$>    <$tracePoint$>
<$endif$><$if fpoint.fault_code$><$if concat(fpoint.param.name)$><$fsimDataMemberInitialize : join(\n)$>
	if(kedr_fsim_point_simulate(fsim_point_<$pointFunctionName$>, &fsim_point_data))
<$else$>    if(kedr_fsim_point_simulate(fsim_point_<$pointFunctionName$>, NULL))
<$endif$>
	{
		<$fpoint.fault_code$>
	}
	else
	{
		/* Call the target function */
		<$targetCall$>
	}
<$else$>    <$targetCall$><$endif$>
<$middleCode$>

<$if trace.happensBefore$><$else$>    <$tracePoint$>
<$endif$>
<$epilogue$>
	return<$if returnType$> returnValue<$endif$>;
}