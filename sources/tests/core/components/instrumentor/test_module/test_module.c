#include <linux/module.h>
#include <linux/init.h>

#include "instrumentor_module.h"

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

static int __init
test_module_init(void)
{
    test_function(5);

    return 0;
}

static void __exit
test_module_exit(void)
{
}

module_init(test_module_init);
module_exit(test_module_exit);