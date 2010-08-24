#ifndef UOBJECT_H
#define UOBJECT_H

/*
 * Auxiliary object.
 * 
 * Besides ref, unref and finalize, when reference count drop to 0,
 * this object support try_use, unuse and invalidate semantic.
 *
 * In invalidate state try_use returns error, and when usage count drop to 0,
 * user-supplied invalidate notifier will be called.
 *
 * Invalidate state may be result of reference count dropped to 0, or
 * of explicite call of invalidate().
 *
 * All object methods, except unref(), unuse() and invalidate(), 
 * may be called in the atomic context.
 *
 * unref(), unuse() and invalidate() methods doesn't call shedule itself,
 * but may call user-defined invalidate and finalize notifier, which may block.
 *
 * After user-supplied notifier is finished, one shouldn't call any object method.
 */
 

#include <linux/spinlock.h>

struct uobject
{
    int refs;
    int usage;
    // Whether invalidate notifier has finished
    int is_invalidated;
    // Protect object data from concurrent use
    spinlock_t ulock;
    // User-supplied notifiers
    void (*invalidate_notifier) (struct uobject* obj);
    void (*finalize_notifier) (struct uobject* obj);
    // Whether invalidate() was explicitely or implicetily called
    int invalidate_was_called;
    // Whether static object was fully destroyed
    int is_finalized;
};
//Initialize struture, and perform first ref().
void uobject_init(struct uobject* obj);

/*
 * Increment reference count of the object.
 *
 * On success return 'obj'.
 *
 * If finalize notifier already was scheduled to call, return NULL,
 * which indicate that reference count cannot be increased on such object.
 */
struct uobject* uobject_ref(struct uobject* obj);

/*
 * Decrement reference count of the object.
 *
 * If reference count drop to 0, implicitely call invalidate.
 * Or, if invalidate notifier has already returned, perform finalize steps.
 */
void uobject_unref(struct uobject* obj);

/* 
 * Increment usage counter of the object.
 * 
 * In such way one can prevent invalidation of the object.
 * (Invalidate notifier doesn't called while usage is not 0.)
 *
 * Return 0 on success, 1 if object already invalidated.
 */
int uobject_try_use(struct uobject* obj);

/*
 * Decrement usage counter of object.
 *
 * If usage counter drop to 0, and invalidate was previousely called,
 * perform invalidation of the object.
 */

void uobject_unuse(struct uobject* obj);

/*
 * Schedule invalidate event, when usage count drop to 0.
 *
 * Return 0 on success, 1 if object has already invalidated.
 */
int uobject_invalidate(struct uobject* obj);

/*
 * Schedule user-supplied function to be called, when invalidation of the
 * object will be performed.
 */
void uobject_set_invalidate_notifier(struct uobject* obj, void (*notifier)(struct uobject* obj));

/*
 * Schedule user-supplied function to be called, when finalization of the
 * object will be performed.
 */
void uobject_set_finalize_notifier(struct uobject* obj, void (*notifier)(struct uobject* obj));

#endif /*UOBJECT_H*/