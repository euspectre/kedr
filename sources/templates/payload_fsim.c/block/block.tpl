//***** Replacement for <$function.name$> *****//

<$if fpoint.reuse_point$><$else$>
// Fault simulation point definition
static struct kedr_simulation_point* fsim_point_<$point_name$>;
#define fsim_point_attributes_<$function.name$> {&fsim_point_<$point_name$>, "<$point_name$>", "<$if concat(fpoint.param.name)$><$fpoint.param.type : join(,)$><$endif$>"}

<$if concat(fpoint.param.name)$>struct fsim_point_data_<$point_name$>
{
	<$fsimDataMember : join(\n\t)$>
};

<$endif$><$endif$>/*
 * 'Simulation' function: return non-zero if need to simulate fault and zero otherwise.
 * Note, that this function also set fault message if needed.
 */
static int kedr_simulate_<$function.name$>(<$argumentSpec_comma$>struct kedr_function_call_info* call_info)
{
	int __result;
	// Extract value of 'caller_address' from replacement function's parameters
	void* caller_address = call_info->return_address;

	<$if concat(fpoint.param.name)$>struct fsim_point_data_<$point_name$> fsim_point_data;

    <$endif$><$if concat(prologue)$><$prologue: join(\n)$>

    <$endif$><$if concat(fpoint.param.name)$><$fsimDataMemberInitialize : join(\n    )$>
	
	<$endif$>__result = kedr_fsim_point_simulate(fsim_point_<$point_name$>, <$if concat(fpoint.param.name)$>&fsim_point_data<$else$>NULL<$endif$>);
	if(__result)
	{
		kedr_fsim_fault_message("%s at [<%p>] %pS", "<$function.name$>", caller_address, caller_address);
	}
	<$if concat(epilogue)$><$epilogue: join(\n)$>

	<$endif$>return __result;
}
// Fault variant of the function
static <$if returnType$><$returnType$><$else$>void<$endif$>
kedr_fault_<$function.name$>(<$argumentSpec_effective$>)
{
	<$if returnType$><$returnType$> ret_val;

	<$endif$><$fpoint.fault_code$>
	
<$if returnType$>	return ret_val;
<$endif$>}

<$if ellipsis$><$if original_code$>
// Original variant of the function which takes 'va_list' argument.
static <$if returnType$><$returnType$><$else$>void<$endif$>
kedr_orig_<$function.name$>(<$argumentSpec_effective$>)
{
	<$if returnType$><$returnType$> ret_val;

	<$endif$><$original_code$>
	
<$if returnType$>	return ret_val;
<$endif$>}
<$else$>
#error 'original_code' parameter should be non-empty for function with variable number of arguments.
<$endif$><$endif$>

// Replacement function itself
static <$if returnType$><$returnType$><$else$>void<$endif$>
kedr_repl_<$function.name$>(<$argumentSpec_comma$>struct kedr_function_call_info* call_info)
{
	<$if returnType$><$returnType$> ret_val;

	<$endif$>int simulate_result;
	{
	<$argsCopy_declare$>
		simulate_result = kedr_simulate_<$function.name$>(<$argumentList_comma$>call_info);
	<$argsCopy_finalize$>}
	{
	<$argsCopy_declare$>
		if(simulate_result)
		{
			<$if returnType$>ret_val = <$endif$>kedr_fault_<$function.name$>(<$argumentList$>);
		}
		else
		{
			<$if returnType$>ret_val = <$endif$><$if ellipsis$>kedr_orig_<$endif$><$function.name$>(<$argumentList$>);
		}
	<$argsCopy_finalize$>}
<$if returnType$>	return ret_val;
<$endif$>}
