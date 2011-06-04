#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

/* Note that support for obtaining reliable stack traces has nothing to do
 * with availability of save_stack_trace() mechanism (controlled by
 * CONFIG_STACKTRACE). If stack traces are reliable but save_stack_trace()
 * cannot be used, there are alternatives but this is another story. */

#if !defined(CONFIG_FRAME_POINTER) && !defined(CONFIG_UNWIND_INFO) && \
    !defined(CONFIG_STACK_UNWIND)
#error Stack trace data can be unreliable on this system.
#endif

/* 
 * The rest of the code does not really matter as long as it is correct 
 * from the compiler's point of view.
 */
static int __init
my_init(void)
{
	return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
