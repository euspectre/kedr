    error = indicator_init_<$indicator.init.name$>(<$indicatorStateName$>);
    if(error) <$if isFailInInit$>goto fail<$else$>return error<$endif$>;