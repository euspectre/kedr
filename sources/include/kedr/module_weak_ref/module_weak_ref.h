#ifndef MODULE_WEAK_REF_H
#define MODULE_WEAK_REF_H

/*
 * May be rename to 'wmodule' ?
 */

/*
 * Implements concept of kernel modules as 'wobj_t' objects.
 *
 * This object is created, when module is loaded, and destroyed,
 * when module is unloaded. Note, that for object destruction variant
 * wobj_unref_final() is used, so module will live until all weak reference callbacks
 * is executed, and anyone doesn't use module via reference(strong)
 *
 * (Compare this with try_module_get() and module_put() kernel mechanism, in which try_module_get()
 * prevent unloading state to start.)
 */

#ifndef __KERNEL__
#error module_weak_ref implement functionality only for kernel space, not for user space.
#endif

#include <linux/module.h> /* struct module definition */
#include <kedr/wobject/wobject.h>

/*
 *  Should be called for use weak reference functionality on module.
 * 
 *  Return 0 on success, not 0 when fail to prepare.
 */

int module_weak_ref_init(void);

/*
 * Should be called when weak reference functionality will no longer be used.
 */

void module_weak_ref_destroy(void);

//reference of module should be performed only via weak reference objects.

//void wmodule_ref(struct module* m);

/*
 * May be safetly used in atomic context, because callback functions
 * of weak references will be done in the process, which unloads module.
 *
 * wmodule_unref() is intended to use after successfull wmodule_weak_ref_get
 * (wmodule_ref() is disabled).
 */

void wmodule_unref(struct module* m);

//reuse weak reference type from wobject
typedef wobj_weak_ref_t wmodule_weak_ref_t;

void wmodule_weak_ref_init(wmodule_weak_ref_t* wmodule_weak_ref, struct module* m,
    void (*destroy_weak_ref)(wmodule_weak_ref_t* wmodule_weak_ref));

struct module* wmodule_weak_ref_get(wmodule_weak_ref_t* wmodule_weak_ref);

void wmodule_weak_ref_clear(wmodule_weak_ref_t* wmodule_weak_ref);

//reuse waiting mechanism from wobject
typedef wobj_wait_callback_t wmodule_wait_callback_t;

void wmodule_wait_callback_prepare(wmodule_wait_callback_t* wait_callback, wmodule_weak_ref_t* wmodule_weak_ref);
void wmodule_wait_callback_wait(wmodule_wait_callback_t* wait_callback);

#endif /* MODULE_WEAK_REF_H */