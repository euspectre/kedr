[group]

function.name = <$function.name$>

<$if returnType$>returnType = <$returnType$>

<$endif$><$if concat(arg.type)$><$arg_def: join(\n)$>

<$endif$><$if trace.happensBefore$>trace.happensBefore = <$trace.happensBefore$>

<$endif$><$if concat(trace.param.type)$><$trace_param_def: join(\n)$>

<$endif$>
<$if trace.formatString$>trace.formatString = <$trace.formatString$>

<$endif$><$if fpoint.fault_code$>
prologue =>>
	void* caller_address;
	<$prologue$>
	caller_address = __builtin_return_address(0);
<<

fpoint.fault_code =>>
	<$fpoint.fault_code$>
<<

<$if concat(fpoint.param.type)$><$fpoint_param_def: join(\n)$>

<$endif$>fpoint.param.type = void*
fpoint.param.name = caller_address
<$else$>
prologue =>>
	<$prologue$>
<<

middleCode =>>
	<$middleCode$>
<<

<$endif$>
<$if middleCode$>middleCode =>>
	<$middleCode$>
<<

<$endif$><$if epilogue$>epilogue =>>
	<$epilogue$>
<<
<$endif$>
