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

static <$returnSpec$>
repl_<$function.name$>(<$argumentSpec$>)
{<$if returnsVoid$><$else$>
	<$returnType$> returnValue;<$endif$>
    <$if concat(fpoint.param.name)$>struct fsim_point_data_<$function.name$> fsim_point_data;<$endif$>
<$prologue$>
<$if concat(fpoint.param.name)$><$fsimDataMemberInitialize : join(\n)$>
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
<$middleCode$>

	<$tracePoint$>

<$epilogue$>
	<$returnStatement$>
}