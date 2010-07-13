spinlock_t lock;
spin_lock_init(&lock);
spin_lock(&lock);
cond_resched_lock(&lock);
spin_unlock(&lock);