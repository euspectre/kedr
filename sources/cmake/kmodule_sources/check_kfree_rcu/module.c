#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>

MODULE_LICENSE("GPL");

struct kedr_foo
{
	struct rcu_head rcu_head;
};

static int __init
my_init(void)
{
	struct kedr_foo *foo;
	
	foo = kmalloc(sizeof(struct kedr_foo), GFP_KERNEL);
	if (foo)
		kfree_rcu(foo, rcu_head);

	return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
