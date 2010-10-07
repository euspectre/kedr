	TP_STRUCT__entry(
		<$if concat(trace.param.name)$><$entryField : join(\n\t\t)$><$else$>__field(int, dummy)<$endif$>
	)