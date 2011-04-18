[group]
function.name = cond_resched_lock
trigger.code =>>
    spinlock_t lock;
    spin_lock_init(&lock);
    spin_lock(&lock);
    cond_resched_lock(&lock);
    spin_unlock(&lock);
<<
