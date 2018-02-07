[group]
function.name = __alloc_pages_nodemask
trigger.code =>>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
	struct page *page = __alloc_pages_nodemask(GFP_KERNEL, 1, 
		numa_node_id(), NULL);
#else
	struct page *page = __alloc_pages_nodemask(GFP_KERNEL, 1, 
		node_zonelist(numa_node_id(), GFP_KERNEL), NULL);
#endif
	if (page)
		__free_pages(page, 1);
<<
