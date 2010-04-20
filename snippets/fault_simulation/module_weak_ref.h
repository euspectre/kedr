#ifndef MODULE_WEAK_REF_H
#define MODULE_WEAK_REF_H

/*
 * Implements weak reference functionality on module.
 */

#include <linux/module.h> /* struct module definition */

typedef void (*destroy_notify)(struct module* m, void* user_data);

/*
 *  Should be called for use weak reference functionality on module.
 * 
 *  Return 0 on success, not 0 when fail to prepare.
 */
int module_weak_ref_init(void);
// Should be called when the functionality will no longer be used.
void module_weak_ref_destroy(void);
// Shedule 'destroy' function to call when module is unloaded.
void module_weak_ref(struct module* m,
	destroy_notify destroy, void* user_data);
// Cancel sheduling.
void module_weak_unref(struct module* m,
	destroy_notify destroy, void* user_data);




#endif /* MODULE_WEAK_REF_H */