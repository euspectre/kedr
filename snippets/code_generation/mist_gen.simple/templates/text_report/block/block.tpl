Name of the function: "<$function.name$>"
<$if concat(arg.name) $>The function takes the following parameters:
- <$param : join(,\n- )$>.
<$else$>The function takes no parameters.
<$endif$>