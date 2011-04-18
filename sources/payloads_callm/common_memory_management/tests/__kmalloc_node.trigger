[group]
function.name = __kmalloc_node
trigger.code =>>
    int size;
    void *p;
    size = 100;
    p = __kmalloc_node(size, GFP_KERNEL, numa_node_id());
    kfree(p);
<<
