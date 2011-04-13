		error = indicator_file_<$indicator.file.name$>_create(<$indicatorStateName$>, control_directory);
		if(error) <$if isIndicatorDestroy$>goto fail<$else$>return error<$endif$>;