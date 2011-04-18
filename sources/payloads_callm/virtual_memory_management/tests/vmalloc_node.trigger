[group]
function.name = vmalloc_node
trigger.code =>>
    void *p = vmalloc_node(100, -1);
    vfree(p);
<<
