	TP_printk("([<%p>] %s+0x%tx)"<$if trace.formatString$>" "
		<$trace.formatString$><$endif$>, 
		__entry->abs_addr, __print_symbolic(__entry->section_id, {0, "unknown"}, {1, "init"}, {2,"core"}), __entry->rel_addr<$if concat(trace.param.name)$>
		, <$entryItem : join(,\n\t\t)$><$endif$>
	)