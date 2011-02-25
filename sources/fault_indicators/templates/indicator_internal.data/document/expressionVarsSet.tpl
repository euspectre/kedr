<$if concat(expression.variable.name)$>        {
#ifdef KEDR_ENABLE_CALLER_ADDRESS
            int current_var_index = 1;
#else
            int current_var_index = 0;
#endif
            <$expressionVarGSet : join(\n            )$>
<$if concat(expression.variable.pname)$>            <$expressionVarPSet : join(\n)$>
<$endif$>        }<$else$>
<$if concat(expression.variable.pname)$>        {
#ifdef KEDR_ENABLE_CALLER_ADDRESS
            int current_var_index = 1;
#else
            int current_var_index = 0;
#endif
            <$expressionVarPSet : join(\n            )$>
        }<$endif$><$endif$>