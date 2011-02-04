int size;
void *p;
size = 100;
p = __kmalloc_node(size, GFP_KERNEL, numa_node_id());
kfree(p);