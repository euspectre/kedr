#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE < KERNEL_VERSION(VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO)
#error Current version is less than required
#endif
static int 
hello_init(void)
{
	return 0;
}

static void 
hello_exit(void)
{
}

module_init(hello_init);
module_exit(hello_exit);
