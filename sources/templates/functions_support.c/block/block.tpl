/* Intermediate replacement function for <$function.name$> */
static struct kedr_intermediate_info kedr_intermediate_info_<$function.name$>;
static <$if returnType$><$returnType$><$else$>void<$endif$> kedr_intermediate_func_<$function.name$>(<$argumentSpec$>)
{
	struct kedr_function_call_info call_info;
	<$if returnType$><$returnType$> ret_val;
	<$endif$>call_info.return_address = __builtin_return_address(0);
	
	// Call all pre-functions.
	if(kedr_intermediate_info_<$function.name$>.pre != NULL)
	{
		void (**pre_function)(<$argumentSpec_comma$>struct kedr_function_call_info* call_info);
		for(pre_function = (typeof(pre_function))kedr_intermediate_info_<$function.name$>.pre;
			*pre_function != NULL;
			++pre_function)
		{
			<$if ellipsis$>va_list args;
			va_start(args, <$last_arg$>);
			<$endif$>(*pre_function)(<$argumentList_comma$>&call_info);<$if ellipsis$>
			va_end(args);<$endif$>
		}
	}
	// Call replacement function
	if(kedr_intermediate_info_<$function.name$>.replace != NULL)
	{
		<$if returnType$><$returnType$><$else$>void<$endif$> (*replace_function)(<$argumentSpec_comma$> struct kedr_function_call_info* call_info) =
			(typeof(replace_function))kedr_intermediate_info_<$function.name$>.replace;
		
		<$if ellipsis$>va_list args;
		va_start(args, <$last_arg$>);
		<$endif$><$if returnType$>ret_val = <$endif$>replace_function(<$argumentList_comma$>&call_info);<$if ellipsis$>
		va_end(args);<$endif$>
	}
	// .. or original one.
	else
	{
		<$if ellipsis$>va_list args;
		va_start(args, <$last_arg$>);
		<$endif$><$originalCode$><$if ellipsis$>
		va_end(args);<$endif$>
	}
	// Call all post-functions.
	if(kedr_intermediate_info_<$function.name$>.post != NULL)
	{
		void (**post_function)(<$argumentSpec_comma$><$if returnType$><$returnType$>, <$endif$>struct kedr_function_call_info* call_info);
		for(post_function = (typeof(post_function))kedr_intermediate_info_<$function.name$>.post;
			*post_function != NULL;
			++post_function)
		{
			<$if ellipsis$>va_list args;
			va_start(args, <$last_arg$>);
			<$endif$>(*post_function)(<$argumentList_comma$><$if returnType$>ret_val, <$endif$>&call_info);<$if ellipsis$>
			va_end(args);<$endif$>
		}
	}
	<$if returnType$>return ret_val;
<$endif$>}
