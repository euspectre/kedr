/* Trigger for function <$function.name$> */
static void
trigger_function_<$function.name$>(<$if trigger.copy_to_user$>void* __user user_area<$else$><$if trigger.copy_from_user$>const void* __user user_area<$else$>void<$endif$><$endif$>)
{
<$trigger.code$>
}
#define trigger_<$function.name$> {\
    .function_name = "<$function.name$>",\
<$if trigger.copy_to_user$>    .type = trigger_type_copy_to_user,\
    { .trigger_function_copy_to_user = trigger_function_<$function.name$>},\
    .user_area_size = <$trigger.copy_to_user.buffer_size$>\
<$else$><$if trigger.copy_from_user$>   .type = trigger_type_copy_from_user,\
    { .trigger_function_copy_from_user = trigger_function_<$function.name$>},\
    .user_area_size = <$trigger.copy_from_user.buffer_size$>\
<$else$>    { .trigger_function = trigger_function_<$function.name$>}\
<$endif$><$endif$>}
