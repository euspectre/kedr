int size;
void* p;
size = 100;
p = __kmalloc(size, GFP_KERNEL);
kfree(p);