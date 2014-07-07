[group]
function.name = _raw_spin_lock_irq
trigger.code =>>
    spinlock_t lock;
    spin_lock_init(&lock);
    spin_lock_irq(&lock);
    spin_unlock_irq(&lock);
<<
