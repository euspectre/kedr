[group]
function.name = vmalloc_32_user
trigger.code =>>
    void *p = vmalloc_32_user(100);
    vfree(p);
<<
