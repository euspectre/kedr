[group]
function.name = vfree
trigger.code =>>
    void *p = vmalloc(100);
    vfree(p);
<<
