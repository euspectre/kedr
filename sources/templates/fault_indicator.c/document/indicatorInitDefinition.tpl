static int
indicator_init_<$indicator.init.name$>(<$if concat(indicator.state.name)$><$indicatorStateDeclaration$><$else$>void<$endif$>)
{
<$indicatorVarsUse$><$indicator.init.code$>
<$indicatorVarsUnuse$>}
