/* 
 * Ready functions for write messages into trace.
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h> /*printk*/

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <kedr/trace/trace.h>

/* 'msg_string' contains some string. */
struct msg_string_data
{
	int len;
	char str[1];
};

static int test_msg_pp(char* dest, size_t size, const void* data)
{
	static const char prefix[] = "test_message_";
#define prefix_size (sizeof(prefix) - 1)
	const struct msg_string_data* ms_data = data;
	
	if(!size) goto out;
	
	if(size <= prefix_size)
	{
		memcpy(dest, prefix, size - 1);
		goto out_truncated;
	}
	
	memcpy(dest, prefix, prefix_size);
	
	if(size <= (prefix_size + ms_data->len))
	{
		memcpy(dest + prefix_size, ms_data->str, size - prefix_size - 1);
		goto out_truncated;
	}
	
	memcpy(dest + prefix_size, ms_data->str, ms_data->len);
	dest[prefix_size + ms_data->len] = '\0';
	goto out;

out_truncated:
	dest[size - 1] = '\0';
out:
	return prefix_size + ms_data->len;
}

void kedr_trace_test_msg_len(const char* str, size_t len)
{
	struct msg_string_data* ms_data;
	
	void* id = kedr_trace_lock(&test_msg_pp,
		offsetof(typeof(*ms_data), str) + len, (void**)&ms_data);
	if(id)
	{
		ms_data->len = len;
		memcpy(ms_data->str, str, len);
		kedr_trace_unlock_commit(id);
	}
}

EXPORT_SYMBOL(kedr_trace_test_msg_len);

static int test_function_call_pp(char* dest, size_t size, const void* data)
{
	const struct msg_string_data* ms_data = data;
	
	if(!size) goto out;
	
	if((int)size <= ms_data->len)
	{
		memcpy(dest, ms_data->str, size - 1);
		dest[size - 1] = '\0';
	}
	
	memcpy(dest, ms_data->str, ms_data->len);
	dest[ms_data->len] = '\0';
out:
	return ms_data->len;
}

void kedr_trace_test_call_msg_len(void* return_address, const char* param, size_t len)
{
	struct msg_string_data* ms_data;
	
	void* id = kedr_trace_function_call_lock("test_function",
		return_address, &test_function_call_pp,
		offsetof(typeof(*ms_data), str) + len, (void**)&ms_data);
	if(id)
	{
		ms_data->len = len;
		memcpy(ms_data->str, param, len);
		kedr_trace_unlock_commit(id);
	}
}
EXPORT_SYMBOL(kedr_trace_test_call_msg_len);


static int __init
trace_generator_init(void)
{
	return 0;
}
static void
trace_generator_exit(void)
{
	kedr_trace_pp_unregister();
}

module_init(trace_generator_init);
module_exit(trace_generator_exit);
