[group]
function.name = kmem_cache_alloc_trace
trigger.code =>>
	void* p;
	struct kmem_cache* mem_cache = kmem_cache_create("kedr_cache", 32, 32, 0, NULL);

	if(mem_cache != NULL)
	{
#if defined(CONFIG_SLAB)
		p = kmem_cache_alloc_trace(32, mem_cache, GFP_KERNEL);
#elif defined(CONFIG_SLUB)
		p = kmem_cache_alloc_trace(mem_cache, GFP_KERNEL, 32);
#else
#error "kmem_cache_alloc_trace() should not be defined"
#endif
		if(p != NULL)
		{
			kmem_cache_free(mem_cache, p);
		}
		else
		{
			printk(KERN_INFO "Cannot allocate object in own kmem_cache.");
		}
		kmem_cache_destroy(mem_cache);
	}
	else
	{
		printk(KERN_INFO "Cannot create kmem_cache.");
	}
<<
