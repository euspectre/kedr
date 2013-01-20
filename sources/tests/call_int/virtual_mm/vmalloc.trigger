[group]
function.name = vmalloc
trigger.code =>>
	void *p = vmalloc(100);
	vfree(p);
<<
