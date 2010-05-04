	TP_fast_assign(
		<$if concat(trace.param.name)$><$entryAssign : join(\n\t\t)$><$else$>__entry->dummy = 0;<$endif$>
	)