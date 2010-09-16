#ifndef MODULE_WEAK_REF_H
#define MODULE_WEAK_REF_H

/*
 * Implements weak reference functionality on kernel modules.
 *
 * module_weak_ref_init() should be executed before all others 
 * functions from this header, module_weak_ref_destroy() should be executed after all,
 * and without any module weak reference currently taken(!).
 *
 * For schedule execution of some function for the moment, when module is unloaded, use
 * module_weak_ref().
 *
 * For cancel scheduling, use module_weak_unref() with same arguments, as for module_weak_ref().
 * It is important, that scheduling is canceled ONLY if this function return 0.
 *
 * Value 1, returned by module_weak_unref(), means that canceling impossible - 
 * given module is currently unloading, and it execute given callback function at that moment.
 * Note, that this is not mean, that callback function is finished at the moment,
 * when module_weak_unref() is returned. Moreover, it is assumed that caller block execution
 * of callback function at some step, e.g. taking mutex, before call of module_weak_unref().
 *
 * If the fact, that callback will be eventually executed in the near moment, 
 * is insufficient for program logic, you should release all mechanismes, which may block callback execution,
 * and call module_weak_ref_wait(). After this function returns, callback is garanteed to be finished.
 *
 * NOTE: without call to module_weak_unref(), call to module_weak_ref_wait() meaningless - 
 * it is required only that this call eventually returns, no more.
 *
 * After call module_weak_unref(), module_weak_ref_wait() is garanteed to wait only callback, that
 * pointed in module_weak_unref() call. But it is correct to call module_weak_ref_wait() after several
 * module_weak_unref() calls - when it returns, all previously pointed callbacks has finished.
 *
 */

#ifndef __KERNEL__
#error module_weak_ref implement functionality only for kernel space, not for user space.
#endif

#include <linux/module.h> /* struct module definition */

/*
 * Type of the callback function, which is called when module is unloaded.
 */

typedef void (*destroy_notify)(struct module* m, void* user_data);

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

/*
 * Schedule 'destroy' function to call when module 'm' will be unloaded.
 *
 * If called while module 'm' is being unloaded(or has already been unloaded),
 * behaviour unspecified.
 *
 * One may assume, that this call is wrapped out by try_module_get(m)/module_put(m).
 *
 * Cannot be used in the interrupt context.
 */

void module_weak_ref(struct module* m,
	destroy_notify destroy, void* user_data);

/*
 * Cancel sheduling.
 *
 * Returns 0, if 'destroy' function will canceling and will not be called in the future.
 * Returns 1, if canceling of 'destroy' function impossible - it is executed at that moment.
 *
 * If called with incorrect parameters(e.g., this callback is already finished to execute),
 * behaviour is unspecified.
 *
 * May be used in the interrupt context.
 */
int module_weak_unref(struct module* m,
	destroy_notify destroy, void* user_data);

/*
 * Block current process until all calls to callbacks,
 * pointed by previous module_weak_unref() calls, has finished.
 */

void module_weak_ref_wait(void);


#endif /* MODULE_WEAK_REF_H */