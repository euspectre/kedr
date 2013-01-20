[group]
function.name = kzfree
trigger.code =>>
	void *p = kmalloc(100, GFP_KERNEL);
	kzfree(p);
<<
