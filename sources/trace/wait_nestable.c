#include "wait_nestable.h"
#include <linux/sched.h>

int wake_flag_function(wait_queue_t* wait, unsigned mode, int sync, void* key)
{
    struct wait_queue_nestable* wqn = container_of(wait, typeof(*wqn), wait);
    
    /* 
     * woken_flag is set AFTER event is set.
     * 
     * So, if wait_flagged_interruptible() reset flag, it should found event
     * occured.
     */
    smp_wmb();
    *wqn->woken_flag_p = 1;
    
    
    
    return autoremove_wake_function(wait, mode, sync, key);
}

void add_wait_queue_nestable(wait_queue_head_t* wq, struct wait_queue_nestable* wqn)
{
    unsigned long flags;
    spin_lock_irqsave(&wq->lock, flags);
    if(list_empty(&wqn->wait.task_list))
        __add_wait_queue(wq, &wqn->wait);
    spin_unlock_irqrestore(&wq->lock, flags);
    /*
     * Need a full(StoreLoad) barrier before futher event checking.
     * 
     * In case of event checking inside mutex critical section
     * (as we actually use this function for kedr trace module),
     * it is sufficient to use smp_wmb() (see smp_mb__before_spinlock()
     * description in the modern kernel).
     * 
     * But use smp_mb() for simplicity and generality.
     */
    smp_mb();
}

int wait_flagged_interruptible(bool* woken_flag_p)
{
    set_current_state(TASK_INTERRUPTIBLE);
    
    if(!(*woken_flag_p) && !signal_pending(current))
        schedule();
    
    __set_current_state(TASK_RUNNING);
    
    /* Reset flag for the next wait iteration. */
    *woken_flag_p = 0;
    /* 
     * All waits should be re-added into waitqueues after that, so
     * no needs for barriers after reseting flag.
     */
    
    return signal_pending(current)? -ERESTARTSYS : 0;
}

