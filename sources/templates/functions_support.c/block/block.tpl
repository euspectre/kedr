/* Trampoline function for <$function.name$> */
static struct kedr_intermediate_info kedr_intermediate_info_<$function.name$>;

<$if ellipsis$><$if original_code$>
// Original variant of the function which takes 'va_list' argument.
static <$if returnType$><$returnType$><$else$>void<$endif$>
kedr_orig_<$function.name$>(<$argumentSpec_effective$>)
{
    <$if returnType$><$returnType$> ret_val;

    <$endif$><$original_code$>
    
<$if returnType$>    return ret_val;
<$endif$>}
<$else$>
#error 'original_code' parameter should be non-empty for function with variable number of arguments.
<$endif$>
/* Intermediate function itself */
<$endif$>
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
<$argsCopy_declare$>
            (*pre_function)(<$argumentList_comma$>&call_info);
<$argsCopy_finalize$>
        }
    }
    // Call replacement function
    if(kedr_intermediate_info_<$function.name$>.replace != NULL)
    {
        <$if returnType$><$returnType$><$else$>void<$endif$> (*replace_function)(<$argumentSpec_comma$> struct kedr_function_call_info* call_info) =
            (typeof(replace_function))kedr_intermediate_info_<$function.name$>.replace;
        
<$argsCopy_declare$>
        <$if returnType$>ret_val = <$endif$>replace_function(<$argumentList_comma$>&call_info);
<$argsCopy_finalize$>
    }
    // .. or original one.
    else
    {
<$argsCopy_declare$>
        <$if returnType$>ret_val = <$endif$><$if ellipsis$>kedr_orig_<$endif$><$function.name$>(<$argumentList$>);
<$argsCopy_finalize$>
    }
    // Call all post-functions.
    if(kedr_intermediate_info_<$function.name$>.post != NULL)
    {
        void (**post_function)(<$argumentSpec_comma$><$if returnType$><$returnType$>, <$endif$>struct kedr_function_call_info* call_info);
        for(post_function = (typeof(post_function))kedr_intermediate_info_<$function.name$>.post;
            *post_function != NULL;
            ++post_function)
        {
<$argsCopy_declare$>
            (*post_function)(<$argumentList_comma$><$if returnType$>ret_val, <$endif$>&call_info);
<$argsCopy_finalize$>
        }
    }
    <$if returnType$>return ret_val;
<$endif$>}
