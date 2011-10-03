    error = indicator_init_<$indicator.init.name$>(<$if concat(indicator.state.name)$><$indicatorStateName$>, <$endif$>params);
    if(error) <$if isFailInInit$>goto fail<$else$>return error<$endif$>;