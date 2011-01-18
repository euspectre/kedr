#include <linux/module.h>
#include <linux/init.h>

#include <linux/ring_buffer.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_DESCRIPTION("Determine signature of ring buffer functions.");
MODULE_LICENSE("GPL");

int test(struct ring_buffer* buffer)
{
	u64 ts;
	struct ring_buffer_event* event =
#if defined(RING_BUFFER_CONSUME_HAS_4_ARGS)
		ring_buffer_consume(buffer, 0, &ts, NULL);
#elif defined(RING_BUFFER_CONSUME_HAS_3_ARGS)
		ring_buffer_consume(buffer, 0, &ts);
#else
#error RING_BUFFER_CONSUME_HAS_4_ARGS or RING_BUFFER_CONSUME_HAS_3_ARGS should be defined.
#endif
	return (event == NULL);

}