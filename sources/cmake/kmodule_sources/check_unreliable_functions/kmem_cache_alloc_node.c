#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/topology.h> /* NUMA-related stuff */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL"); 

static void do_something(void);

static int __init
probe_init_module(void)
{
	do_something();
	return 0;
}

static void __exit
probe_exit_module(void)
{
	return;
}

module_init(probe_init_module);
module_exit(probe_exit_module);

static void
do_something(void)
{
	void *p;
	struct kmem_cache* mem_cache = kmem_cache_create("kedr_cache", 32, 32, 0, NULL);

	if(mem_cache != NULL)
	{
		p = kmem_cache_alloc_node(mem_cache, GFP_KERNEL, numa_node_id());
		if(p != NULL)
		{
			kmem_cache_free(mem_cache, p);
		}
		else
		{
			printk(KERN_INFO "Cannot allocate object in own kmem_cache.");
		}
		kmem_cache_destroy(mem_cache);
	}
	else
	{
		printk(KERN_INFO "Cannot create kmem_cache.");
	}
}
/* end of source */ 
