static <$returnSpec$>
repl_<$function.name$>(<$argumentSpec$>)
{<$if returnsVoid$><$else$>
	<$returnType$> returnValue;<$endif$>
<$prologue$>
	
	/* Call the target function */
	<$targetCall$>

<$middleCode$>

	<$tracePoint$>

<$epilogue$>
	<$returnStatement$>
}