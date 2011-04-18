#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/topology.h> /* NUMA-related stuff */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL"); 

static int __init
probe_init_module(void)
{
	int size;
	void *p;
	size = 100;
	p = __kmalloc_node(size, GFP_KERNEL, numa_node_id());
	kfree(p);

	return 0;
}

static void __exit
probe_exit_module(void)
{
	return;
}

module_init(probe_init_module);
module_exit(probe_exit_module);
