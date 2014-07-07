[group]
function.name = vmalloc_32
trigger.code =>>
	void *p = vmalloc_32(100);
	vfree(p);
<<
