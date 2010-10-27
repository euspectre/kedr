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
	unsigned long stack_entry;
	struct stack_trace trace = {
		.nr_entries = 0,
		.entries = &stack_entry,
		.max_entries = 1,
		.skip = 2
	};
	<$if fpoint.fault_code$><$if concat(fpoint.param.name)$>
	struct fsim_point_data_<$function.name$> fsim_point_data;<$endif$><$endif$>

<$prologue$>
	// Determine caller address (absolute and relative in the section)
	save_stack_trace(&trace);
	abs_addr = (void*)stack_entry;
	if((target_core_addr != NULL) && (abs_addr >= target_core_addr) && (abs_addr < target_core_addr + target_core_size))
	{
		section_id = 2;
		rel_addr = abs_addr - target_core_addr;
	}
	else if((target_init_addr != NULL) && (abs_addr >= target_init_addr) && (abs_addr < target_init_addr + target_init_size))
	{
		section_id = 1;
		rel_addr = abs_addr - target_init_addr;
	}
	else
	{
		section_id = 0;
		rel_addr = abs_addr - (void*)0;
	}

<$if fpoint.fault_code$><$if concat(fpoint.param.name)$><$fsimDataMemberInitialize : join(\n)$>
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

	<$tracePoint$>

<$epilogue$>
	return<$if returnType$> returnValue<$endif$>;
}