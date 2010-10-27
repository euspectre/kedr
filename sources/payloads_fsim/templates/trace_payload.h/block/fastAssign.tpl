	TP_fast_assign(
		__entry->abs_addr = abs_addr;
		__entry->section_id = section_id;
		__entry->rel_addr = rel_addr;<$if concat(trace.param.name)$>
		<$entryAssign : join(\n\t\t)$><$endif$>
	)