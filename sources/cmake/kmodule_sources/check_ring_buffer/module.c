#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");


#if !defined(CONFIG_RING_BUFFER)
#error Ring buffer is not supported by the system.
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
