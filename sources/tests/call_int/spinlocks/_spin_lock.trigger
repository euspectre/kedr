[group]
function.name = _spin_lock
trigger.code =>>
    spinlock_t lock;
    spin_lock_init(&lock);
    spin_lock(&lock);
    spin_unlock(&lock);
<<
