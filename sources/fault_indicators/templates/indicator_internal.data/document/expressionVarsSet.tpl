<$if concat(expression.variable.name)$>        {
            int current_var_index = 0;
            <$expressionVarGSet : join(\n            )$>
<$if concat(expression.variable.pname)$>            <$expressionVarPSet : join(\n)$>
<$endif$>        }<$else$>
<$if concat(expression.variable.pname)$>        {
            int current_var_index = 0;
            <$expressionVarPSet : join(\n            )$>
        }<$endif$><$endif$>