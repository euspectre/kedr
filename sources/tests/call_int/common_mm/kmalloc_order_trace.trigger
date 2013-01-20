[group]
function.name = kmalloc_order_trace
trigger.code =>>
	size_t size;
	void *p;
	size = 100;
	p = kmalloc_order_trace(size, GFP_KERNEL, 1);
	kfree(p);
<<
