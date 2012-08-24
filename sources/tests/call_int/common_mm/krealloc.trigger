[group]
function.name = krealloc
trigger.code =>>
    void* p = krealloc(NULL, 100, GFP_KERNEL);
    kfree(p);
<<
