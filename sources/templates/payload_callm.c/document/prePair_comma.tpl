#ifdef KEDR_PRE_<$function.name$>
	{
		.orig = (void*)&<$function.name$>,
		.pre = (void*)&kedr_pre_<$function.name$>
	},
#endif
