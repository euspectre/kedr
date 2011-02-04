int size;
void* p;
size = 100;
p = alloc_pages_exact(size, GFP_KERNEL);
if (p) 
	free_pages_exact(p, size);