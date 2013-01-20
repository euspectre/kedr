[group]
function.name = alloc_pages_exact_nid
trigger.code =>>
	size_t size;
	void* p;
	size = 100;
	p = alloc_pages_exact_nid(numa_node_id(), size, GFP_KERNEL);
	if (p) 
		free_pages_exact(p, size);
<<
