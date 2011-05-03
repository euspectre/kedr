#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/slab.h>

MODULE_LICENSE("GPL");

#if defined(IS_ALLOCATOR_SLAB) && defined(CONFIG_SLAB)
//#message "SLAB allocator is used."
#elif defined(IS_ALLOCATOR_SLUB) && defined(CONFIG_SLUB)
//#message "SLUB allocator is used."
#elif defined(IS_ALLOCATOR_SLOB) && defined(CONFIG_SLOB)
//#message "SLOB allocator is used."
#else
#error "Unknown allocator request."
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
