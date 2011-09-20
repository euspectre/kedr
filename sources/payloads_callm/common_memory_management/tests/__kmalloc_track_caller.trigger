[group]
function.name = __kmalloc_track_caller
trigger.code =>>
    int size;
    void *p;
    size = 100;
    p = __kmalloc_track_caller(size, GFP_KERNEL, 
        (unsigned long)__builtin_return_address(0));
    kfree(p);
<<
