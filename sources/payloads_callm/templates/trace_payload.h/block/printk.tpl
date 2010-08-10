	TP_printk(<$trace.formatString$><$if concat(trace.param.name)$>,
		<$entryItem : join(,\n\t\t)$><$else$> "%s", ""<$endif$>
	)