#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>

MODULE_LICENSE("GPL");

struct my_item
{
	struct hlist_node hlist;
	int val;
};

static struct hlist_head head;
static struct my_item item;

static int __init
my_init(void)
{
	struct my_item *pos;
	
	INIT_HLIST_HEAD(&head);
	hlist_add_head(&item.hlist, &head);
	
	hlist_for_each_entry(pos, &head, hlist) {
		pos->val = 0;
	}
	return 0;
}

static void __exit
my_exit(void)
{
	struct my_item *pos;
	hlist_for_each_entry(pos, &head, hlist) {
		pr_info("%d\n", pos->val);
	}
}

module_init(my_init);
module_exit(my_exit);
