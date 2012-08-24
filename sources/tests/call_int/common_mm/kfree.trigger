[group]
function.name = kfree
trigger.code =>>
    void* p = kmalloc(100, GFP_KERNEL);
    kfree(p);
<<
