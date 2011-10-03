static int
indicator_init_<$indicator.init.name$>(<$if concat(indicator.state.name)$><$indicatorStateDeclaration$>, <$endif$>const char* params)
{
<$indicatorVarsUse$><$indicator.init.code$>
<$indicatorVarsUnuse$>}
