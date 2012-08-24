[group]
function.name = __mutex_init
trigger.code =>>
    struct mutex m;
    mutex_init(&m);
    mutex_destroy(&m);
<<
