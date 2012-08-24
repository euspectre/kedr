[group]
function.name = __kmalloc_node_track_caller
trigger.code =>>
    int size;
    void *p;
    size = 100;
    p = __kmalloc_node_track_caller(size, GFP_KERNEL, numa_node_id(),
        (unsigned long)__builtin_return_address(0));
    kfree(p);
<<
