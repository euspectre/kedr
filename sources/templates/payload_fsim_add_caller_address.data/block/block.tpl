# Add support of 'caller_address' fault simulation point parameter.
[group]

function.name = <$function.name$>

<$if returnType$>returnType = <$returnType$>

<$endif$><$if concat(arg.name)$><$argDefinition: join(\n)$>

<$endif$><$if ellipsis$>ellipsis = <$ellipsis$>

last_arg = <$last_arg$>

original_code =>>
<$original_code$>
<<

<$endif$><$if fpoint.fault_code$>fpoint.fault_code =>>
<$fpoint.fault_code$>
<<
<$endif$># Add 'caller_address' parameter to the fault simulation point.
fpoint.param.type = void*
fpoint.param.name = caller_address

<$if args_copy_init$>args_copy_init =>>
    <$args_copy_init$>
<<

<$endif$><$if args_copy_destroy$>args_copy_destroy =>>
    <$args_copy_destroy$>
<<

<$endif$><$if concat(fpoint.param.name)$><$fpointParam: join(\n)$>

<$endif$><$if concat(prologue)$><$prologueSection: join(\n)$>

<$endif$><$if concat(epilogue)$><$epilogueSection: join(\n)$>

<$endif$><$if fpoint.reuse_point$>fpoint.reuse_point = <$fpoint.reuse_point$>

<$endif$><$if fpoint.rename$>fpoint.rename = <$fpoint.rename$>

<$endif$><$if message.formatString$>message.formatString = <$message.formatString$>

<$if message.param.type$><$messageParamDefinition: join(\n)$>

<$endif$><$endif$>
