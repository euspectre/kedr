static int
indicator_simulate_<$indicator.simulate.name$>(<$if concat(indicator.state.name)$><$indicatorStateDeclaration$><$if concat(indicator.parameter.name)$>,
	<$pointDataDeclaration$><$endif$><$if indicator.simulate.first$>,
	int *never<$endif$><$else$><$if concat(indicator.parameter.name)$><$pointDataDeclaration$><$if indicator.simulate.first$>,
	int *never<$endif$><$else$><$if indicator.simulate.first$>int *never<$else$>void<$endif$><$endif$><$endif$>)
{
<$indicatorVarsUse$><$if indicator.simulate.first$>#define simulate_never() do {*never = 1; } while(0)
<$endif$><$pointDataUse$><$indicator.simulate.code$>
<$pointDataUnuse$><$if indicator.simulate.first$>#undef simulate_never
<$endif$><$indicatorVarsUnuse$>}