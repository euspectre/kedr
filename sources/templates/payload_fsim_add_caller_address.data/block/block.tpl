# Add support of 'caller_address' fault simulation point parameter.
[group]

function.name = <$function.name$>

<$if returnType$>returnType = <$returnType$>

<$endif$><$if concat(arg.name)$><$argDefinition: join(\n)$>

<$endif$><$if fpoint.fault_code$>fpoint.fault_code =>>
<$fpoint.fault_code$>
<<
<$endif$># Add 'caller_address' parameter to the fault simulation point.
fpoint.param.type = void*
fpoint.param.name = caller_address

<$if concat(fpoint.param.name)$><$fpointParam: join(\n)$>

<$endif$># Extract value of 'caller_address' from replacement function's parameters
prologue = void* caller_address = call_info->return_address;

<$if concat(prologue)$><$prologueSection: join(\n)$>

<$endif$><$if concat(epilogue)$><$epilogueSection: join(\n)$>

<$endif$><$if fpoint.reuse_point$>fpoint.reuse_point = <$fpoint.reuse_point$>

<$endif$>
