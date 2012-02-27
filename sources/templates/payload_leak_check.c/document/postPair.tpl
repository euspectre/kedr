#ifdef KEDR_LEAK_CHECK_POST_<$function.name$>
	{
		.orig = (void*)&<$function.name$>,
		.post  = (void*)&post_<$function.name$>
	},
#endif
