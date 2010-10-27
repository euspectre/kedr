	TP_STRUCT__entry(
		__field(void*, abs_addr)
		__field(int, section_id)
		__field(ptrdiff_t, rel_addr)<$if concat(trace.param.name)$>
		<$entryField : join(\n\t\t)$><$endif$>
	)