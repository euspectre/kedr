[group]
function.name = vzalloc_node
trigger.code =>>
	void *p = vzalloc_node(100, numa_node_id());
	vfree(p);
<<
