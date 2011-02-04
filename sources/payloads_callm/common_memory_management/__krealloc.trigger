void* p = __krealloc(NULL, 100, GFP_KERNEL);
kfree(p);