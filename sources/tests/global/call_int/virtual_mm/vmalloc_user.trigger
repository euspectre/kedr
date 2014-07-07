[group]
function.name = vmalloc_user
trigger.code =>>
	void *p = vmalloc_user(100);
	vfree(p);
<<
