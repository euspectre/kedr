<$if fpoint.fault_code$>
<$if fpoint.reuse_point$>
//Registration of point should't occures.
#define fsim_point_<$function.name$> fake_fsim_point
#define fsim_point_name_<$function.name$> NULL
#define fsim_point_format_<$function.name$> NULL
#define fsim_point_data_<$function.name$> fsim_point_data_<$fpoint.reuse_point$>
<$else$>
static struct kedr_simulation_point* fsim_point_<$function.name$>;
#define fsim_point_name_<$function.name$> "<$function.name$>"
#define fsim_point_format_<$function.name$> "<$fpoint.param.type : join(,)$>"
    <$if concat(fpoint.param.name)$>
struct fsim_point_data_<$function.name$>
{
<$fsimDataMember : join(\n)$>
};
	<$endif$>
<$endif$>
<$endif$>
static <$if returnType$><$returnType$><$else$>void<$endif$>
repl_<$function.name$>(<$argumentSpec$>)
{<$if returnType$>
	<$returnType$> returnValue;<$endif$>
	void *abs_addr; 
	ptrdiff_t rel_addr;
	int section_id;
	<$if fpoint.fault_code$><$if concat(fpoint.param.name)$>
	struct fsim_point_data_<$function.name$> fsim_point_data;<$endif$><$endif$>

<$prologue$>
	// Determine caller address (absolute and relative in the section)
	get_caller_address(abs_addr, section_id, rel_addr);

<$if trace.happensBefore$><$if returnType$>#error "Happens before relationship cannot be used for functions which return values."
<$endif$>    <$tracePoint$>
<$endif$><$if fpoint.fault_code$><$if concat(fpoint.param.name)$><$fsimDataMemberInitialize : join(\n)$>
	if(kedr_fsim_point_simulate(fsim_point_<$function.name$>, &fsim_point_data))
<$else$>    if(kedr_fsim_point_simulate(fsim_point_<$function.name$>, NULL))
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