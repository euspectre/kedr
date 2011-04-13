static void
indicator_destroy_<$indicator.destroy.name$>(<$if isIndicatorState$><$indicatorStateDeclaration$><$else$>void<$endif$>)
{
<$indicatorVarsUse$><$indicator.destroy.code$>
<$indicatorVarsUnuse$>}