<$if indicator.simulate.first$><$else$>    result = indicator_simulate_<$indicator.simulate.name$>(<$if concat(indicator.state.name)$><$indicatorStateName$><$if concat(indicator.parameter.name)$>,
	<$pointDataName$><$endif$><$else$><$if concat(indicator.parameter.name)$><$pointDataName$><$endif$><$endif$>);
    if(result) return result;
<$endif$>