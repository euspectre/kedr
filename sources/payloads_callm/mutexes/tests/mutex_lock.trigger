[group]
function.name = mutex_lock
trigger.code =>>
    struct mutex m;
    mutex_init(&m);
    mutex_lock(&m);
    mutex_unlock(&m);
    mutex_destroy(&m);
<<
