#include <linux/module.h>
#include <linux/init.h>

#include <linux/ring_buffer.h>

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_DESCRIPTION("Determine the signature of ring_buffer_resize().");
MODULE_LICENSE("GPL");

int test(struct ring_buffer* buffer)
{
	int ret =
#if defined(RING_BUFFER_RESIZE_BUF_SIZE)
		ring_buffer_resize(buffer, 128);
#elif defined(RING_BUFFER_RESIZE_BUF_SIZE_CPU)
		ring_buffer_resize(buffer, 128, RING_BUFFER_ALL_CPUS);
#else
#error RING_BUFFER_RESIZE_BUF_SIZE or RING_BUFFER_RESIZE_BUF_SIZE_CPU must be defined.
#endif
	return ret;

}