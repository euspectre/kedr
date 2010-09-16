#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */

#include <kedr/module_weak_ref/module_weak_ref.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

int current_value = 5;
module_param(current_value, int, S_IRUGO);

void set_value_to_10(struct module* m, void* user_data)
{
    (void)m;
    (void)user_data;

    current_value = 10;
}
//exported function(example of usage of 'module_weak_ref' functionality)
void kedr_i_am_here(struct module* m)
{
    module_weak_ref(m, set_value_to_10, NULL);
}
EXPORT_SYMBOL(kedr_i_am_here);

//exported function(example of usage of 'module_weak_unref' functionality)
void kedr_i_am_not_here(struct module* m)
{
    if(module_weak_unref(m, set_value_to_10, NULL))
        printk(KERN_INFO "Concurrent unrefing.");
}
EXPORT_SYMBOL(kedr_i_am_not_here);


static int __init
module_a_init(void)
{
    module_weak_ref_init();
    return 0;
}

static void
module_a_exit(void)
{
    module_weak_ref_destroy();
	return;
}

module_init(module_a_init);
module_exit(module_a_exit);