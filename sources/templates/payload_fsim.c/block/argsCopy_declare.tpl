<$if concat(arg.name)$><$argCopy_declare: join()$><$if ellipsis$>va_list args_copy;
    <$endif$><$if args_copy_init$><$args_copy_init$>
    <$endif$><$if ellipsis$>va_copy(args_copy, args);
    <$endif$><$endif$>