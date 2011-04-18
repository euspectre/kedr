<$function.name$>)
<$if trigger.copy_to_user$>copy_to_user="yes"
user_buffer_size=<$trigger.copy_to_user.buffer_size$>
<$else$><$if trigger.copy_from_user$>copy_from_user="yes"
user_buffer_size=<$trigger.copy_from_user.buffer_size$>
<$endif$><$endif$>;;
