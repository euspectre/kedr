//***** Replacement for <$function.name$> *****//

<$if fpoint.reuse_point$><$else$>
// Fault simulation point definition
static struct kedr_simulation_point* fsim_point_<$point_name$>;
#define fsim_point_attributes_<$function.name$> {&fsim_point_<$point_name$>, "<$point_name$>", "<$if concat(fpoint.param.name)$><$fpoint.param.type : join(,)$><$endif$>"}

<$if concat(fpoint.param.name)$>struct fsim_point_data_<$point_name$>
{
	<$fsimDataMember : join(\n\t)$>
};

<$endif$><$endif$>// Replacement function itself
static <$if returnType$><$returnType$><$else$>void<$endif$>
kedr_repl_<$function.name$>(<$if concat(arg.name)$><$argument : join(, )$>,
	<$endif$>struct kedr_function_call_info* call_info)
{
// Extract value of 'caller_address' from replacement function's parameters
	void* caller_address = call_info->return_address;
<$if returnType$>	<$returnType$> returnValue;

<$endif$><$if concat(fpoint.param.name)$>	struct fsim_point_data_<$point_name$> fsim_point_data;

<$endif$><$if concat(prologue)$><$prologue: join(\n)$>

<$endif$><$if concat(fpoint.param.name)$>	<$fsimDataMemberInitialize : join(\n\t)$>
<$endif$>
	if(kedr_fsim_point_simulate(fsim_point_<$point_name$>, <$if concat(fpoint.param.name)$>&fsim_point_data<$else$>NULL<$endif$>))
	{
		<$fpoint.fault_code$>
		kedr_fsim_fault_message(<$if message.formatString$><$message.formatString$><$if concat(message.param.type)$>, <$messageParam: join(, )$><$endif$><$else$>"%s at [<%p>] %pS", "<$function.name$>", caller_address, caller_address<$endif$>);
	}
	else
	{
		/* Call the target function */
		<$if returnType$>returnValue = <$endif$><$function.name$>(<$if concat(arg.name)$><$arg.name : join(, )$><$endif$>);
	}
<$if concat(epilogue)$><$epilogue: join(\n)$>

<$endif$>    (void)caller_address;//for supress warnings about unused variable

<$if returnType$>	return returnValue;
<$endif$>}