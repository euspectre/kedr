[group]
function.name = kmem_cache_alloc_trace
trigger.code =>>
	void *p;
	struct kmem_cache* mem_cache = 
		kmem_cache_create("kedr_mem_cache", 32, 32, 0, NULL);

	if (mem_cache != NULL)
	{
#if defined(KMCA_TRACE_SIZE_FIRST)
		p = kmem_cache_alloc_trace(32, mem_cache, GFP_KERNEL);
#elif defined(KMCA_TRACE_KMC_FIRST)
		p = kmem_cache_alloc_trace(mem_cache, GFP_KERNEL, 32);
#else
#  error "Unspecified order of the arguments of kmem_cache_alloc*_trace()"
#endif
		if (p != NULL)
		{
			kmem_cache_free(mem_cache, p);
		}
		else
		{
			printk(KERN_INFO 
			"Cannot allocate object from our own kmem_cache.");
		}
		kmem_cache_destroy(mem_cache);
	}
	else
	{
		printk(KERN_INFO "Cannot create kmem_cache.");
	}
<<
