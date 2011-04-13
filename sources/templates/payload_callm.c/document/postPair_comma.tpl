#ifdef KEDR_POST_<$function.name$>
	{
		.orig = (void*)&<$function.name$>,
		.post = (void*)&kedr_post_<$function.name$>
	},
#endif
