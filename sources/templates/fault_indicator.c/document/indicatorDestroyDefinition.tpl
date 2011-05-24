static void
indicator_destroy_<$indicator.destroy.name$>(<$if concat(indicator.state.name)$><$indicatorStateDeclaration$><$else$>void<$endif$>)
{
<$indicatorVarsUse$><$indicator.destroy.code$>
<$indicatorVarsUnuse$>}