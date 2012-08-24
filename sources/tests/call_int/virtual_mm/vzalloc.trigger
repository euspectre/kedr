[group]
function.name = vzalloc
trigger.code =>>
    void *p = vzalloc(100);
    vfree(p);
<<
