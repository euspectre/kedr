# Modify initial content of the data-file for indicator to add support of 'caller_address' variable.

module.author = <$module.author$>
module.license = <$module.license$>
indicator.name = <$indicator.name$>

# Add 'caller_address' parameter of the indicator
indicator.parameter.type = void*
indicator.parameter.name = caller_address

<$if concat(indicator.parameter.type)$><$indicatorParameter: join(\n)$>

<$endif$><$if concat(global)$><$globalSection: join(\n)$>

<$endif$><$if concat(expression.constant.c_name)$><$expressionCConstant: join(\n)$>

<$endif$><$if concat(expression.constant.name)$><$expressionConstant: join(\n)$>

<$endif$># Add 'caller_address' variable to the expression
expression.variable.name = caller_address
expression.variable.value = (unsigned long)caller_address

<$if concat(expression.variable.pname)$><$expressionPVariable: join(\n)$>

<$endif$><$if concat(expression.variable.name)$><$expressionVariable: join(\n)$>

<$endif$><$if concat(expression.rvariable.name)$><$expressionRVariable: join(\n)$>

<$endif$>
