void *p = __vmalloc(100, GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL);
vfree(p);