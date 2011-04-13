//***** Replacement for <$function.name$> *****//

<$if fpoint.reuse_point$><$else$>
// Fault simulation point definition
static struct kedr_simulation_point* fsim_point_<$function.name$>;
#define fsim_point_attributes_<$function.name$> {&fsim_point_<$function.name$>, "<$function.name$>", "void*<$if concat(fpoint.param.type)$>,<$fpoint.param.type : join(,)$><$endif$>"}
struct fsim_point_data_<$function.name$>
{
	void* return_address;
<$if concat(fpoint.param.name)$>	<$fsimDataMember : join(\n\t)$>
<$endif$>};

// Replacement function itself
<$endif$>static <$if returnType$><$returnType$><$else$>void<$endif$>
kedr_repl_<$function.name$>(<$if concat(arg.name)$><$argument : join(, )$>,
	<$endif$>struct kedr_function_call_info* call_info)
{
<$if returnType$>	<$returnType$> returnValue;

<$endif$>	struct fsim_point_data_<$if fpoint.reuse_point$><$fpoint.reuse_point$><$else$><$function.name$><$endif$> fsim_point_data;

<$if concat(prologue)$><$prologue: join(\n)$>

<$endif$>	fsim_point_data.return_address = call_info->return_address;
<$if concat(fpoint.param.name)$>	<$fsimDataMemberInitialize : join(\n\t)$>
<$endif$>
	if(kedr_fsim_point_simulate(fsim_point_<$if fpoint.reuse_point$><$fpoint.reuse_point$><$else$><$function.name$><$endif$>, &fsim_point_data))
	{
		<$fpoint.fault_code$>
	}
	else
	{
		/* Call the target function */
		<$if returnType$>returnValue = <$endif$><$function.name$>(<$if concat(arg.name)$><$arg.name : join(, )$><$endif$>);
	}
<$if concat(epilogue)$><$epilogue: join(\n)$>

<$endif$><$if returnType$>	return returnValue;
<$endif$>}