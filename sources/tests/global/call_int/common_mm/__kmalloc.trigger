[group]
# Name of the target function
function.name = __kmalloc
	
# The code to trigger a call to this function.
trigger.code =>>
	size_t size = 20;
	void *p;
	p = __kmalloc(size, GFP_KERNEL);
	kfree(p);
<<
#######################################################################
