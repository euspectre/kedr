#include <linux/module.h>
#include <linux/init.h>

#include <linux/ring_buffer.h>
#include <linux/smp.h>

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_DESCRIPTION("Determine the signature of ring_buffer_size().");
MODULE_LICENSE("GPL");

int test(struct ring_buffer* buffer)
{
	unsigned long s =
#if defined(RING_BUFFER_SIZE_BUF)
		ring_buffer_size(buffer);
#elif defined(RING_BUFFER_SIZE_BUF_CPU)
		ring_buffer_size(buffer, smp_processor_id());
#else
#error RING_BUFFER_SIZE_BUF or RING_BUFFER_SIZE_BUF_CPU must be defined.
#endif
	return (s != 0);

}