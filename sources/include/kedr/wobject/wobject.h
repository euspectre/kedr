#ifndef KEDR_WOBJECT
#define KEDR_WOBJECT

#ifndef __KERNEL__
#error "This header is for kernel code only."
#endif

#include <linux/list.h>
#include <linux/spinlock.h> /* spinlocks */
#include <linux/wait.h> /* for wait_queue_head_t pointer definition */

//Forward declarations
typedef struct wobj wobj_t;
typedef struct wobj_weak_ref wobj_weak_ref_t;
typedef struct wobj_wait_callback wobj_wait_callback_t;

/*
 * Type, describing object with reference counting and weak reference functionality.
 */

struct wobj
{
    //All fields are private.
    int refs;
    spinlock_t lock;//protect all fields from concurrent reads and writes
    struct list_head weak_refs;//list of weak references pointed to this object
    struct list_head callback_waiters;//
    wait_queue_head_t* final_waiter;//NULL if no wobj_unref_final() was called
    void (*finalize)(wobj_t* obj);
};

/*
 * Initialize fields of wobj_t structure.
 * Set reference counter to be 1.
 *
 * May be used in atomic context.
 */

void wobj_init(wobj_t* obj, void (*finalize)(wobj_t* obj));

/*
 * Increment reference count on object, thus preventing finalize function to be called even after 'unref'.
 *
 * Should be used, when reference count of object is not 0.
 *
 * May be used in atomic context.
 */

wobj_t* wobj_ref(wobj_t* obj);

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

void wobj_unref(wobj_t* obj);

/*
 * Decrement reference counting on object.
 *
 * Wait until reference count on object drop to 0, and call 'finalize' method.
 *
 * NOTE: only one call of wobj_unref_final() is allowed for object.
 */
void wobj_unref_final(wobj_t* obj);

/*
 * Type, describing weak reference to the 'wobj_t' object.
 */

struct wobj_weak_ref
{
    //All fields are private
    struct list_head list;
    spinlock_t lock;//protect all fields from concurrent writes in wobj_weak_ref_get() and object finalization mechanizm.
    void (*destroy) (wobj_weak_ref_t* wobj_weak_ref);
    wobj_t* obj;//reference to object
    wobj_t* obj_shadow;//for implementation of wobject_wait_callback_prepare()
};

/*
 * Initialize fields of wobj_weak_ref_t structure
 * and bind it with 'obj'.
 *
 * If not NULL, 'destroy_weak_ref' function will be called when reference count to 'obj' drop to 0,
 * but before its 'finalize' function will be called.
 *
 * NOTE: Should be called when reference count of the 'obj' is not 0.
 *
 * May be used in atomic context.
 */

void wobj_weak_ref_init(wobj_weak_ref_t* wobj_weak_ref, wobj_t* obj, void (*destroy_weak_ref)(wobj_weak_ref_t* wobj_weak_ref));

/*
 * Return object, to which this weak reference points out. Also, increment reference count to the object.
 *
 * If 'destroy_weak_ref' already started to execute, because reference count of object became 0, return NULL.
 */

wobj_t* wobj_weak_ref_get(wobj_weak_ref_t* wobj_weak_ref);

/*
 * Break out binding of 'wobj_weak_ref' with object. This cancel callback execution.
 *
 * NOTE: Should be called when reference count of the 'obj' is not 0.
 *
 * May be used in atomic context.
 */

void wobj_weak_ref_clear(wobj_weak_ref_t* wobj_weak_ref);

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
 * Type of object, which may be used to wait, when callback of wobj_weak_ref has finish to execute.
 */

struct wobj_wait_callback
{
    struct list_head list;
    int is_finished;
    wait_queue_head_t waitqueue;
    wait_queue_t wait;
};

/*
 * Prepare to wait, until callback, registered by 'wobj_weak_ref', has finished.
 *
 * NOTE: This function should be called AFTER
 * get() method for 'wobj_weak_ref' returns NULL and BEFORE
 * callback, registered by this weak reference, has not finished yet.(e.g, under corresponding spinlock taken).
 *
 * May be used in atomic context.
 */

void wobj_wait_callback_prepare(wobj_wait_callback_t* wait_callback, wobj_weak_ref_t* wobj_weak_ref);

/*
 * Wait, until callback, registered by 'wobj_weak_ref', has finished.
 *
 * If this callback has already finish, return immediately.
 *
 * After this function, 'wait_callback' object may be reused for other waiting, or may be freed.
 *
 * NOTE: should be used by the same thread, which call wobj_wait_callback_prepare() for this 'wait_callback' object.
 */

void wobj_wait_callback_wait(wobj_wait_callback_t* wait_callback);

#endif /* KEDR_WOBJECT */