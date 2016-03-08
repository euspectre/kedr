#ifndef KEDR_TARGET_DETECTOR_INTERNAL_H
#define KEDR_TARGET_DETECTOR_INTERNAL_H

/*
 * Interface of KEDR functionality responsible for detection of
 * loading/unloading of the target module.
 */

#include <linux/module.h> /* 'struct module' definition */

/* 
 * These two callbacks are called when target module is loading/unloading.
 * 
 * Should be implemented elsewhere.
 */
extern int on_target_load(struct module* m);
extern void on_target_unload(struct module* m);

/*
 * This callback is called when detector should watch for several
 * targets.
 * 
 * On success, should return 0.
 * If watching for several target is not supported, should return -err.
 * 
 * NOTE: Function may be called before any initialization code.
 */
int force_several_targets(void);

/* 
 * This callback is called when detector watch for one or zero targets.
 * 
 * NOTE: Function may be called before any initialization code.
 */
void unforce_several_targets(void);


/*
 * Initialize detector.
 */
int kedr_target_detector_init(void);

/*
 * Destroy detector.
 */
void kedr_target_detector_destroy(void);

/*
 * Set the name of the target module, that is module which loading and
 * and unloading should be detected.
 * Several names may be given, separated with ','.
 * Empty string means no targets.
 * 
 * If at least one of the current or new targets is loaded, return error.
 * 
 * NOTE: Function is allowed to be called before any initialization code.
 */
int kedr_target_detector_set_target_name(const char* target_name);

/*
 * Fill buffer with names of target modules.
 * 
 * Return number of characters written or negative error code.
 */
int kedr_target_detector_get_target_name(char* buf, size_t size);

#endif /* KEDR_TARGET_DETECTOR_INTERNAL_H */
