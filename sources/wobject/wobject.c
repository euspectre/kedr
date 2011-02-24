/* ========================================================================
 * Copyright (C) 2010-2011, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <kedr/wobject/wobject.h>

#include <linux/wait.h> /* for wait */
#include <linux/sched.h> /* schedule */


/*
 * Performs all finalize actions(when refcount is 0).
 *
 * NOTE: Should be called under spinlock taken, but release spinlock itself.
 *
 * If this functions return 0, then finalize function for object was called, and
 * object shouldn't be accessed at all.
 *
 * If function returns not 0, then finalization was interrupted,
 * because some weak reference increment refcount of the object.
*/
static int wobj_finalize(wobj_t* obj, unsigned long flags);

///////////////Implementation of exported functions//////////////////////////

/*
 * Initialize fields of wobj_t structure.
 * Reference counting set to be 1.
 *
 * May be used in atomic context.
 */

void wobj_init(wobj_t* obj, void (*finalize)(wobj_t* obj))
{
    obj->refs = 1;
    spin_lock_init(&obj->lock);
    INIT_LIST_HEAD(&obj->weak_refs);
    INIT_LIST_HEAD(&obj->callback_waiters);
    obj->final_waiter = NULL;
    obj->finalize = finalize;
}

/*
 * Increment reference count on object, thus preventing finalize function to be called even after 'unref'.
 *
 * Should be used, when reference count of object is not 0.
 *
 * May be used in atomic context.
 */
wobj_t* wobj_ref(wobj_t* obj)
{
    unsigned long flags;
    spin_lock_irqsave(&obj->lock, flags);
    BUG_ON(obj->refs == 0);
    obj->refs++;
    spin_unlock_irqrestore(&obj->lock, flags);
    return obj;
}

/*
 * Decrement reference counting on object.
 *
 * If reference counting drop to 0, call 'finalize' method of the object(if wobj_unref_final wasn't be called).
 *
 * May be used in atomic context, if:
 *  -drop reference, which is not last one
 *  -wobj_unref_final() was called previously
 *  -user-defined functions 'finalize' and weak references callbacks may be used in atomic conext itselves.
 */

void wobj_unref(wobj_t* obj)
{
    unsigned long flags;
    spin_lock_irqsave(&obj->lock, flags);
    BUG_ON(obj->refs == 0);
    obj->refs--;
    if(obj->refs == 0)
    {
        if(obj->final_waiter)
        {
            wake_up_interruptible(obj->final_waiter);
        }
        else
        {
            wobj_finalize(obj, flags);
            return;
        }
    }
    spin_unlock_irqrestore(&obj->lock, flags);
}

/*
 * Decrement reference counting on object.
 *
 * Wait until reference count on object drop to 0, and call 'finalize' method.
 *
 * NOTE: only one call of wobj_unref_final() is allowed for object.
 */

void wobj_unref_final(wobj_t* obj)
{
    unsigned long flags;

    DEFINE_WAIT(wait);
    DECLARE_WAIT_QUEUE_HEAD(waitqueue);
    
    prepare_to_wait(&waitqueue, &wait, TASK_INTERRUPTIBLE);

    spin_lock_irqsave(&obj->lock, flags);
    BUG_ON(obj->refs == 0);
    BUG_ON(obj->final_waiter != NULL);
    obj->final_waiter = &waitqueue;
    obj->refs--;

    while(1)
    {
        while(obj->refs != 0)
        {
            spin_unlock_irqrestore(&obj->lock, flags);
            schedule();
            spin_lock_irqsave(&obj->lock, flags);
        }
        
        if(!wobj_finalize(obj, flags))
        {
            finish_wait(&waitqueue, &wait);
            break;
        }
        //reacqure lock after finalize
        spin_lock_irqsave(&obj->lock, flags);
    }
}



/*
 * Initialize fields of wobj_weak_ref_t structure
 * and bind it with 'obj'.
 *
 * So, 'destroy_weak_ref' function will be called when reference count to 'obj' drop to 0,
 * but before its 'finalize' function will be called.
 *
 * 'destroy_weak_ref' function will be called in the same process, as 'finalize' function of the object.
 *
 * NOTE: This function should be called when reference count of the 'obj' is not 0.
 *
 * May be used in atomic context.
 */

void wobj_weak_ref_init(wobj_weak_ref_t* wobj_weak_ref, wobj_t* obj, void (*destroy_weak_ref)(wobj_weak_ref_t* wobj_weak_ref))
{
    unsigned long flags;
    wobj_weak_ref->destroy = destroy_weak_ref;
    //may be initialized with NULL object
    if(obj == NULL)
    {
        wobj_weak_ref->obj = NULL;
        wobj_weak_ref->obj_shadow = NULL;
        return;
    }
    spin_lock_init(&wobj_weak_ref->lock);
    
    spin_lock_irqsave(&obj->lock, flags);
    BUG_ON(obj->refs == 0);
    wobj_weak_ref->obj = obj;
    wobj_weak_ref->obj_shadow = obj;
    list_add(&wobj_weak_ref->list, &obj->weak_refs);
    spin_unlock_irqrestore(&obj->lock, flags);
}

/*
 * Return object, to which this weak reference points out. Also, increment reference count to the object.
 *
 * If 'destroy_weak_ref' already started to execute, because reference count of object became 0, return NULL.
 */

wobj_t* wobj_weak_ref_get(wobj_weak_ref_t* wobj_weak_ref)
{
    unsigned long flags;
    wobj_t* obj;
    spin_lock_irqsave(&wobj_weak_ref->lock, flags);
    obj = wobj_weak_ref->obj;
    if(obj != NULL)
    {
        unsigned long flags1;
        spin_lock_irqsave(&obj->lock, flags1);
        obj->refs++;
        spin_unlock_irqrestore(&obj->lock, flags1);
    }
    spin_unlock_irqrestore(&wobj_weak_ref->lock, flags);
    return obj;
}

/*
 * Break out binding of 'wobj_weak_ref' with object. This cancel callback execution.
 *
 * NOTE: This function should be called when reference count of the 'obj' is not 0.
 *
 * May be used in atomic context.
 */

void wobj_weak_ref_clear(wobj_weak_ref_t* wobj_weak_ref)
{
    unsigned long flags;
    unsigned long flags1;
    wobj_t* obj;
    spin_lock_irqsave(&wobj_weak_ref->lock, flags1);
    obj = wobj_weak_ref->obj;
    BUG_ON(obj == NULL);//weak reference dead, so it cannot be used for changing object state
    
    spin_lock_irqsave(&obj->lock, flags);
    BUG_ON(obj->refs == 0);
    list_del(&wobj_weak_ref->list);
    spin_unlock_irqrestore(&obj->lock, flags);
    
    spin_unlock_irqrestore(&wobj_weak_ref->lock, flags1);
}

/*
 * Next functionality is intended for situation, when 'destroy_weak_ref' function destroy 'wobj_weak_ref' object itself.
 *
 * In that situation, before accessing to 'wobj_weak_ref' object, one should use mechanism, which pause 
 * 'destroy_weak_ref' before it will remove object. E.g., this may be spinlock, or mutex.
 *
 * At this stage, wobj_weak_ref_get() returns NULL('destroy_weak_ref' already starts to execute), but one cannot remove
 * wobj_weak_ref oneself, because after 'destroy_weak_ref' continue to work, it try to repeat this removing.
 * So, one may simply omit removing stage, letting 'destroy_weak_ref' function to do it by itself, or may wait until
 * 'destroy_weak_ref' function remove object.
 * Next functions are intend for use waiting approach.
 */


/*
 * Prepare to wait, until callback, registered by 'wobj_weak_ref', has finished.
 *
 * NOTE: This function should be called AFTER
 * get() method for 'wobj_weak_ref' returns NULL and BEFORE
 * callback has not finished yet.(e.g, under corresponding spinlock or mutex taken).
 *
 * May be used in atomic context.
 */

void wobj_wait_callback_prepare(wobj_wait_callback_t* wait_callback, wobj_weak_ref_t* wobj_weak_ref)
{
    unsigned long flags;
    wobj_t* obj;
    
    wait_callback->is_finished = 0;
    init_waitqueue_head(&wait_callback->waitqueue);
    init_wait(&wait_callback->wait);
    prepare_to_wait(&wait_callback->waitqueue, &wait_callback->wait, TASK_INTERRUPTIBLE);

    BUG_ON(wobj_weak_ref->obj != NULL);//cannot be used with live weak reference
    obj = wobj_weak_ref->obj_shadow;

    spin_lock_irqsave(&obj->lock, flags);
    list_add(&wait_callback->list, &obj->callback_waiters);
    spin_unlock_irqrestore(&obj->lock, flags);
}

/*
 * Wait, until callback, registered by 'wobj_weak_ref', has finished.
 *
 * If this callback has already finish, return immediately.
 *
 * After this function, 'wait_callback' object may be reused for other waiting, or may be freed.
 *
 * NOTE: should be used by the same thread, which call wobj_wait_callback_prepare() for this 'wait_callback' object.
 */

void wobj_wait_callback_wait(wobj_wait_callback_t* wait_callback)
{
    while(!wait_callback->is_finished)
        schedule();
    finish_wait(&wait_callback->waitqueue, &wait_callback->wait);
}

//////////////////Implementation of auxiliary functions///////////////////////
int wobj_finalize(wobj_t* obj, unsigned long flags)
{
    
    while(!list_empty(&obj->weak_refs))
    {
        unsigned long flags1;
        wobj_weak_ref_t* weak_ref = list_first_entry(&obj->weak_refs, wobj_weak_ref_t, list);
        
        
        if(!spin_trylock_irqsave(&weak_ref->lock, flags1))
        {
/* 
 * Someone use this weak reference.
 * But the only method, which is allowed to use weak reference when reference count of object is 0,
 * is wobj_weak_ref_get, which wish increment reference count on object.
 * So, interrupt object destruction.
 *
 * NOTE!!! We CANNOT use normal spin_lock, because weak reference user acquires weak reference's spinlock first,
 * and only after this acquire object's spinlock. We acquire spinlocks in reverse order.
 *
 */
            spin_unlock_irqrestore(&obj->lock, flags);
            return 1;
        }
        BUG_ON(weak_ref->obj != obj);
/*
 * Break out binding of weak reference with object.
 *
 * weak_ref->obj_shadow does not cleared, because it is used for waiting callback,
 * and it is user, who had to provide correctness of wait mechanism usage.
 */
        weak_ref->obj = NULL;
        spin_unlock_irqrestore(&weak_ref->lock, flags1);

        list_del(&weak_ref->list);
        
        if(weak_ref->destroy)
        {
            /*
             * This trick prevents repeated call of wobj_finalize as result of repeated drop refcount to 0.
             * Also, callback function of weak reference 'as if' has ownership over the object(but it hasn't access it:) ).
             */
            obj->refs++;

            // Call destroy function WITHOUT object spinlock
        
            spin_unlock_irqrestore(&obj->lock, flags);
            weak_ref->destroy(weak_ref);
            spin_lock_irqsave(&obj->lock, flags);
            
            //wakeup callback waiters, if they are
            while(!list_empty(&obj->callback_waiters))
            {
                wobj_wait_callback_t* wait_callback = list_first_entry(&obj->callback_waiters, wobj_wait_callback_t, list);
                list_del(&wait_callback->list);
                wait_callback->is_finished = 1;
                wake_up_interruptible_all(&wait_callback->waitqueue);
            }
            // restore refcounting
            obj->refs--;

            if(obj->refs != 0)
            {
                //someone ref object from existing weak reference
                spin_unlock_irqrestore(&obj->lock, flags);
                return 1;
            }
        }
    }
    spin_unlock_irqrestore(&obj->lock, flags);
    if(obj->finalize)
        obj->finalize(obj);
    return 0;
}