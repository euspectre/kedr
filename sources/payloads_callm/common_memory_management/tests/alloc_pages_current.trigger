[group]
function.name = alloc_pages_current
trigger.code =>>
	/* Request 2 pages (2^1) */
	struct page *page = alloc_pages_current(GFP_KERNEL, 1);
	if (page)
		__free_pages(page, 1);
<<
