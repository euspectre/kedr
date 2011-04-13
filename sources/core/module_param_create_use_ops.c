#include <linux/module.h>
#include <linux/moduleparam.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_DESCRIPTION("Create module parameter using operations.");
MODULE_LICENSE("GPL");

static int
some_param_get(char* buffer, struct kernel_param *kp)
{
    buffer[1] = 'A';
    return 1;
}

static int
some_param_set(const char* val, struct kernel_param *kp)
{
    return 0;
}

module_param_call(some_param,
    some_param_get, some_param_set,
    NULL,
    S_IRUGO | S_IWUSR);
