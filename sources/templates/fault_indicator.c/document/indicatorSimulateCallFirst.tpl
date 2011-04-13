<$if indicator.simulate.first$>    result = indicator_simulate_<$indicator.simulate.name$>(<$if isIndicatorState$><$indicatorStateName$>,
<$endif$><$if isPointData$>        <$pointDataName$>,
<$endif$>        &never);
    if(result || never) return result;<$endif$>