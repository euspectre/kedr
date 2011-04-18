[group]
function.name = __alloc_pages_nodemask
trigger.code =>>
	struct page *page = __alloc_pages_nodemask(GFP_KERNEL, 1, 
		node_zonelist(numa_node_id(), GFP_KERNEL), NULL);
	if (page)
		__free_pages(page, 1);
<<
