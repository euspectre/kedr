<$if indicator.simulate.first$><$else$>    result = indicator_simulate_<$indicator.simulate.name$>(<$if isIndicatorState$><$indicatorStateName$><$if isPointData$>,
	    <$endif$><$endif$><$pointDataName$>);
    if(result) return result;<$endif$>