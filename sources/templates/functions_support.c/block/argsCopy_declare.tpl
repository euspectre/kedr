<$if concat(arg.name)$>        <$if ellipsis$>va_list args_copy;
        <$endif$><$if args_copy_declare_and_init$><$args_copy_declare_and_init$>
        <$else$><$argCopy_declare: join(\n        )$>
        <$endif$><$if ellipsis$>va_start(args_copy, <$last_arg$>);
<$endif$><$endif$>