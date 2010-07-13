spinlock_t lock;
spin_lock_init(&lock);
spin_lock_irq(&lock);
spin_unlock_irq(&lock);