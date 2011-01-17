module.author = <$module.author$>
module.license = <$module.license$>

indicator.name = <$indicator.name$>

<$if concat(global)$><$global_def: join(\n)$>
<$endif$><$if concat(indicator.parameter.type)$><$indicator_parameter_def: join(\n)$>
<$endif$>indicator.parameter.type = void*
indicator.parameter.name = caller_address

<$if concat(expression.variable.name)$><$expression_variable_def: join(\n)$>

<$endif$><$if concat(expression.variable.pname)$><$expression_variable_p_def: join(\n)$>
<$endif$>expression.variable.name = caller_address
expression.variable.value = (kedr_calc_int_t)caller_address

<$if concat(expression.rvariable.name)$><$expression_rvariable_def: join(\n)$>

<$endif$><$if concat(expression.constant.name)$><$expression_constant_def: join(\n)$>

<$endif$><$if concat(expression.constant.cname)$><$expression_constant_c_def: join(\n)$>
<$endif$>
