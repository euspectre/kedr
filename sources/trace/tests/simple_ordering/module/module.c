#include <linux/module.h>
#include <linux/init.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <kedr/trace/trace.h>

static int pp_int(char* dest, size_t size, const void* n)
{
	return snprintf(dest, size, "test_message %d", *(const int*)n);
}

static void simple_trace(int n)
{
	kedr_trace(pp_int, &n, sizeof(n));
}

static int __init
som_init(void)
{
	int i;
	for(i = 0; i < 5; i++)
		simple_trace(i);

	return 0;
}
static void
som_exit(void)
{
	kedr_trace_pp_unregister();
}

module_init(som_init);
module_exit(som_exit);
