#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/string.h> /* strcmp() */

#include <kedr/base/common.h>

char* function_name = NULL;
module_param(function_name, charp, S_IRUGO);

int is_intercepted = 0;
module_param(is_intercepted, int, S_IRUGO);

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

<$include: join(\n)$>

<$block : join(\n)$>

struct interception
{
	const char* function_name;
	void* target_function;
	void* replacement_function;
};

static const struct interception
interceptions[] =
{
	<$interception: join(\n\t)$>
	{NULL, NULL, NULL}
};

static struct kedr_payload interception_payload = {
    .mod                    = THIS_MODULE,
    .repl_table.orig_addrs  = NULL, //will be filled with target_function address
    .repl_table.repl_addrs  = NULL, //will be filled with replacement_function address
    .repl_table.num_addrs   = 1,
    .target_load_callback   = NULL,
    .target_unload_callback = NULL
};

static int __init
payload_init(void)
{
	int result;
	const struct interception* interception;
	if(function_name == NULL)
	{
		pr_err("'function_name' parameter should be set for module when it is inserted.");
		return -EINVAL;
	}
	for(interception = interceptions; interception->function_name != NULL; interception++)
	{
		if(strcmp(interception->function_name, function_name) == 0)
			break;
	}
	if(interception->function_name == NULL)
	{
		pr_err("Replacement is not exist for function '%s'.", function_name);
		return -EINVAL;
	}
	//really this both pointers are used as 'const void**'
	interception_payload.repl_table.orig_addrs =
		(void**)&interception->target_function;
	interception_payload.repl_table.repl_addrs =
		(void**)&interception->replacement_function;
	
	result = kedr_payload_register(&interception_payload);
	if(result)
	{
		pr_err("Cannot register payload module for verify call interception.");
	}
	return result;
}

static void
payload_exit(void)
{
	kedr_payload_unregister(&interception_payload);
}

module_init(payload_init);
module_exit(payload_exit);