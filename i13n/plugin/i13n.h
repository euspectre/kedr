#ifndef I13N_H_1230_INCLUDED
#define I13N_H_1230_INCLUDED

/* 
 * How many arguments of a called function to pass to the handlers
 * (at most).
 */
#define KEDR_NR_ARGS 7

/* 
 * kedr_function_class
 * A group of functions that should be handled the same way and have the
 * needed arguments at the same positions.
 * 
 * For example, 
 *   void *__kmalloc(size_t, gfp_t) and 
 *   void *kmalloc_order(size_t size, gfp_t flags, unsigned int order)
 * may belong to the same class as long as only the size (size_t) and
 * the flags (gfp_t) are needed.
 *
 * Each function class has one pre- and one post-handler associated with it.
 * These functions will be called before and after the call to the function
 * of this class.
 */
struct kedr_function_class
{
	/* 
	 * Which arguments of the called function to pass to the handlers.
	 * Each number is a position of a given argument in the list of
	 * arguments for that function.
	 * The positions start from 1.
	 * 0 marks the end of the list.
	 * The arguments will be cast to unsigned long if necessary and 
	 * will be passed to the handlers in the order their positions are
	 * listed here.
	 * 
	 * [NB] If an argument of the target function is not an integer or
	 * a pointer, e.g. a struct passes by value, its address will be
	 * passed to the handler instead of the value.
	 */
	unsigned char arg_pos[KEDR_NR_ARGS + 1];

	/*
	 * true if the post handler needs the return value of the function,
	 * false otherwise.
	 */
	bool need_ret;

	/* 
	 * Names of the pre-handler and the post-handler.
	 * These handlers will be called before and after the function of 
	 * this class is, respectively.
	 *
	 * A pre-handler should have the following signature:
	 * void <name_pre>([unsigned long arg1, ..., unsigned long argN],
	 * 		   void *lptr);
	 * arg1 - argN are the arguments of the "target" function of this
	 * class as specified in arg_pos[].
	 *
	 * A post-handler should have the following signature:
	 * void <name_post>([unsigned long ret], void *lptr);
	 * 
	 * Note that the post-handler does not receive the arguments of the
	 * target function directly. If they are needed there, then it is
	 * the job of the pre-handler to save them somewhere, e.g. in the
	 * instance of struct kedr_local lptr points to. This will make them
	 * available in the post-handler.
	 */
	const char *name_pre;
	const char *name_post;

	/* DECLs for the handlers that can be used to generate the calls. */
	tree decl_pre;
	tree decl_post;
};

/*
 * Returns a pointer to the kedr_function_class instance for a function with
 * the given name if found, NULL if not.
 * 
 * If the function returns the instance, the instance will also have
 * 'decl_pre' and 'decl_post' set.
 */
const kedr_function_class *
kedr_get_class_by_fname(const std::string & fname);

/* Set the common properties of a function decl. */
void
kedr_set_fndecl_properties(tree fndecl);

#endif /*I13N_H_1230_INCLUDED*/
