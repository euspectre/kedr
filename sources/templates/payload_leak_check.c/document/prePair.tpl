#ifdef KEDR_LEAK_CHECK_PRE_<$function.name$>
	{
		.orig = (void*)&<$function.name$>,
		.pre  = (void*)&pre_<$function.name$>
	},
#endif
