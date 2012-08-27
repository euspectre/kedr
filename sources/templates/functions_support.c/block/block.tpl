/* Intermediate replacement function for <$function.name$> */
static struct kedr_intermediate_info kedr_intermediate_info_<$function.name$>;
static <$if returnType$><$returnType$><$else$>void<$endif$> kedr_intermediate_func_<$function.name$>(<$argumentSpec$>)
{
	struct kedr_function_call_info call_info;
	<$if returnType$><$returnType$> result;
	<$endif$>call_info.return_address = __builtin_return_address(0);
	
	// Call all pre-functions.
	if(kedr_intermediate_info_<$function.name$>.pre != NULL)
	{
		void (**pre_function)(<$argumentSpec_comma$>struct kedr_function_call_info* call_info);
		for(pre_function = (typeof(pre_function))kedr_intermediate_info_<$function.name$>.pre;
			*pre_function != NULL;
			++pre_function)
		{
			(*pre_function)(<$argumentList_comma$>&call_info);
		}
	}
	// Call replacement function
	if(kedr_intermediate_info_<$function.name$>.replace != NULL)
	{
		<$if returnType$><$returnType$><$else$>void<$endif$> (*replace_function)(<$argumentSpec_comma$> struct kedr_function_call_info* call_info) =
			(typeof(replace_function))kedr_intermediate_info_<$function.name$>.replace;
		
		<$if returnType$>result = <$endif$>replace_function(<$argumentList_comma$>&call_info);
	}
	// .. or original one.
	else
	{
		<$if returnType$>result = <$endif$><$function.name$>(<$argumentList$>);
	}
	// Call all post-functions.
	if(kedr_intermediate_info_<$function.name$>.post != NULL)
	{
		void (**post_function)(<$argumentSpec_comma$><$if returnType$><$returnType$>, <$endif$>struct kedr_function_call_info* call_info);
		for(post_function = (typeof(post_function))kedr_intermediate_info_<$function.name$>.post;
			*post_function != NULL;
			++post_function)
		{
			(*post_function)(<$argumentList_comma$><$if returnType$>result, <$endif$>&call_info);
		}

	}
	<$if returnType$>return result;
<$endif$>}