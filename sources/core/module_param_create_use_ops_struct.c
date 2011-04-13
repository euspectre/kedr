#include <linux/module.h>
#include <linux/moduleparam.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_DESCRIPTION("Create module parameter using operations structure.");
MODULE_LICENSE("GPL");

static int
some_param_get(char* buffer, const struct kernel_param *kp)
{
    buffer[1] = 'A';
    return 1;
}

static int
some_param_set(const char* val, const struct kernel_param *kp)
{
    return 0;
}

static struct kernel_param_ops some_param_ops =
{
    .set = some_param_set,
    .get = some_param_get,
};
module_param_cb(some_param,
    some_param_ops,
    NULL,
    S_IRUGO | S_IWUSR);
