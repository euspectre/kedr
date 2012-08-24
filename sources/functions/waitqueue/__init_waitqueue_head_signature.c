#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/wait.h>   /* wait-related functions */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_DESCRIPTION("Determine the signature of __init_waitqueue_head().");
MODULE_LICENSE("GPL");

void test(void)
{
	wait_queue_head_t q;
	static struct lock_class_key __key;
#if defined(INIT_WAITQUEUE_HEAD_NAME)
	__init_waitqueue_head(&q, "q", &__key);
#elif defined(INIT_WAITQUEUE_HEAD_OLD)
	__init_waitqueue_head(&q, &__key);
#else
#error INIT_WAITQUEUE_HEAD_OLD or INIT_WAITQUEUE_HEAD_NAME must be defined.
#endif
	pr_info("[DBG] wait queue address: %p\n", &q);
	return;
}