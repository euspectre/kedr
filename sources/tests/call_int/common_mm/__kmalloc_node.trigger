[group]
function.name = __kmalloc_node
trigger.code =>>
	size_t size;
	void *p;
	size = 100;
	p = __kmalloc_node(size, GFP_KERNEL, numa_node_id());
	kfree(p);
<<
