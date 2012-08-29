/* Interception of the <$function.name$> function */
<$if handler.pre$>#define KEDR_LEAK_CHECK_PRE_<$function.name$>
void pre_<$function.name$>(<$argumentSpec_comma$>struct kedr_function_call_info* call_info)
{
	void* caller_address = call_info->return_address;
<$handler.pre$>
}
<$endif$><$if handler.post$>#define KEDR_LEAK_CHECK_POST_<$function.name$>
void post_<$function.name$>(<$argumentSpec_comma$><$if returnType$><$returnType$> ret_val, <$endif$>struct kedr_function_call_info* call_info)
{
	void* caller_address = call_info->return_address;
<$handler.post$>
}
<$endif$>