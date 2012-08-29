<$if indicator.simulate.first$>    result = indicator_simulate_<$indicator.simulate.name$>(<$if concat(indicator.state.name)$><$indicatorStateName$>,
		<$endif$><$if concat(indicator.parameter.name)$><$pointDataName$>,
		<$endif$>&never);
	if(result || never) return result;
<$endif$>