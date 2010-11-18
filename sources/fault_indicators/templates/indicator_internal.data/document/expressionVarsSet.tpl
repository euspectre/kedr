<$if concat(expression.vars.name)$>        {
            int current_var_index = 0;
            <$expressionVarGSet : join(\n            )$>
<$if concat(expression.vars.pname)$>            <$expressionVarPSet : join(\n)$>
<$endif$>        }<$else$>
<$if concat(expression.vars.pname)$>        {
            int current_var_index = 0;
            <$expressionVarPSet : join(\n            )$>
        }<$endif$><$endif$>