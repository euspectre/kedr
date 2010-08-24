/*
 * Implementation of uobject.
 */

#include <kedr/syscall_connector/uobject.h>

/*
 * 1. Invalidate state, after call of invalidate() (explicitly or implicitly).
 *      -invalidate_was_called
 *      -try_use() return error from this moment
 * 2. Invalidate state, after usage count drop to 0, before the end of the call of invalidate notifier.
 *      -usage == 0
 *      -set_invalidate_notifier() do nothing from this moment
 * 3. Invalidate state, after the call of invalidate notifier.
 *      -is_invalidated
 *      -
 * 4. Finalize state, after reference count drop to 0, before the memory, used by object, was freed in the finalize notifier.
 *      -refs == 0
 *      -ref() return error from this moment, set_finalize_notifier() do nothing from this moment
 * 5(optional). Finalize state, after the memory, used by object, was freed in the finalize notifier.
 *      -(object is not exist)
 *      -no function may be used after this moment
 * 5'(optional). Finalize state, finalize notifier doesn't set.
 *      -is_finalized
 *      -no function may be used after this moment
 */


//Auxiliary functions, for reuse code

/*
 * Call(or schedule) invalidate notifier, and perform next steps.
 */

static void uobject_invalidate_notify(struct uobject* obj);

/*
 * Call(or schedule) finalize notifier. After call of this function object may not exist.
 */

static void uobject_finalize_notify(struct uobject* obj);

/*
 * Same as invalidate, but should be executed under lock taken.
 *
 * On success, release lock.
 */

int uobject_invalidate_internal(struct uobject* obj, unsigned long flags);

//Initialize struture, and perform first ref().
void uobject_init(struct uobject* obj)
{
    obj->refs = 1;
    obj->usage = 0;
    //
    obj->is_invalidated = 0;
    //
    spin_lock_init(&obj->ulock);
    //
    obj->invalidate_notifier = NULL;
    obj->finalize_notifier = NULL;
    //
    obj->invalidate_was_called = 0;
    //
    obj->is_finalized = 0;
}

struct uobject* uobject_ref(struct uobject* obj)
{
    unsigned long flags;
    struct uobject* result = NULL;
    BUG_ON(obj->is_finalized);

    spin_lock_irqsave(&obj->ulock, flags);
    //uobject_ref() return success up to the scheduling finalize notifier(state 4)
    if(obj->refs || !obj->is_invalidated)
    {
        obj->refs++;
        result = obj;
    }
    spin_unlock_irqrestore(&obj->ulock, flags);

    return result;
}
void uobject_unref(struct uobject* obj)
{
    unsigned long flags;

    BUG_ON(obj->is_finalized);

    spin_lock_irqsave(&obj->ulock, flags);
    BUG_ON(obj->refs <= 0);
    obj->refs--;

    if(obj->refs == 0)
    {
        if(uobject_invalidate_internal(obj, flags) == 0)
            return;//lock was released inside uobject_invalidate_internal()
        //object was already in invalidate state(2)
        if(obj->is_invalidated)
        {
            //object in finalize state(4)
            spin_unlock_irqrestore(&obj->ulock, flags);
            uobject_finalize_notify(obj);
            return;
        }
    }
    spin_unlock_irqrestore(&obj->ulock, flags);
}
/* 
 * Prevent invalidation of the object.
 * (Invalidate notifier doesn't called while usage is not 0.)
 *
 * Return 0 on success, 1 if object in invalidate state.
 */
int uobject_try_use(struct uobject* obj)
{
    unsigned long flags;
    int result = 0;

    BUG_ON(obj->is_finalized);

    spin_lock_irqsave(&obj->ulock, flags);
    if(!obj->invalidate_was_called)
        obj->usage++;
    else
        result = 1;//object is in invalidate state(1)
    spin_unlock_irqrestore(&obj->ulock, flags);
    
    return result;
}
void uobject_unuse(struct uobject* obj)
{
    unsigned long flags;

    BUG_ON(obj->is_finalized);

    spin_lock_irqsave(&obj->ulock, flags);
    BUG_ON(obj->usage <= 0);
    obj->usage--;
    if(obj->invalidate_was_called && (obj->usage == 0))
    {
        //object is in invalidate state(2)
        spin_unlock_irqrestore(&obj->ulock, flags);
        uobject_invalidate_notify(obj);
        return;
    }
    spin_unlock_irqrestore(&obj->ulock, flags);
}
/*
 * Schedule invalidate event, when usage count drop to 0.
 *
 * Return 0 on success, 1 if object has already invalidated.
 */

int uobject_invalidate(struct uobject* obj)
{
    unsigned long flags;

    BUG_ON(obj->is_finalized);

    spin_lock_irqsave(&obj->ulock, flags);
    if(uobject_invalidate_internal(obj, flags) == 0)
        return 0;
    spin_unlock_irqrestore(&obj->ulock, flags);
    return 1;
}
void uobject_set_invalidate_notifier(struct uobject* obj, void (*notifier)(struct uobject* obj))
{
    unsigned long flags;

    BUG_ON(obj->is_finalized);

    spin_lock_irqsave(&obj->ulock, flags);
    //set notifier if it was not scheduled
    if(!obj->invalidate_was_called || obj->usage)
    {
        //object is up to the state 2
        obj->invalidate_notifier = notifier;
    }
    spin_unlock_irqrestore(&obj->ulock, flags);

}
void uobject_set_finalize_notifier(struct uobject* obj, void (*notifier)(struct uobject* obj))
{
    unsigned long flags;

    BUG_ON(obj->is_finalized);

    spin_lock_irqsave(&obj->ulock, flags);
    //set notifier if it was not scheduled
    if(obj->refs || !obj->is_invalidated)
    {
        //object is up to the state 4
        obj->finalize_notifier = notifier;
    }
    spin_unlock_irqrestore(&obj->ulock, flags);
}

//Auxiliary functions implementation
static void uobject_finalize_notify(struct uobject* obj)
{
    if(obj->finalize_notifier)
    {
        obj->finalize_notifier(obj);
    }
    else
        obj->is_finalized = 1;//for static object prevent futhure methods calls
}
//
static void uobject_invalidate_notify(struct uobject* obj)
{
    unsigned long flags;
    int need_finalize = 0;
    
    if(obj->invalidate_notifier)
    {
        obj->invalidate_notifier(obj);
    }
    spin_lock_irqsave(&obj->ulock, flags);

    obj->is_invalidated = 1;
    if(obj->refs == 0)
        need_finalize = 1;

    spin_unlock_irqrestore(&obj->ulock, flags);

    if(need_finalize)
        uobject_finalize_notify(obj);
}
//
int uobject_invalidate_internal(struct uobject* obj, unsigned long flags)
{
    int need_invalidate = 0;
    if(obj->invalidate_was_called) return 1;

    obj->invalidate_was_called = 1;
    
    if(obj->usage == 0)
        need_invalidate = 1;
    spin_unlock_irqrestore(&obj->ulock, flags);

    if(need_invalidate)
        uobject_invalidate_notify(obj);
    return 0;
}