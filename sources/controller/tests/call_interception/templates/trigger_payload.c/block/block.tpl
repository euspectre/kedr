static <$if returnType$><$returnType$><$else$>void<$endif$>
<$function.name$>__repl(<$if concat(arg.type)$><$arg_def: join(,)$><$else$>void<$endif$>)
{
	is_intercepted = 1;
	<$if returnType$>return	<$endif$><$function.name$>(<$if concat(arg.type)$><$arg.name: join(,)$><$endif$>);
}