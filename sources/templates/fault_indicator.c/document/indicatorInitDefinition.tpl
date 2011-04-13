static int
indicator_init_<$indicator.init.name$>(<$if isIndicatorState$><$indicatorStateDeclaration$><$else$>void<$endif$>)
{
<$indicatorVarsUse$><$indicator.init.code$>
<$indicatorVarsUnuse$>}