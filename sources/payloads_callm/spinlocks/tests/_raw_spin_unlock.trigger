[group]
function.name = _raw_spin_unlock
trigger.code =>>
    spinlock_t lock;
    spin_lock_init(&lock);
    spin_lock(&lock);
    spin_unlock(&lock);
<<
