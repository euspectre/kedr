#ifndef KEDR_TARGET_DETECTOR_INTERNAL_H
#define KEDR_TARGET_DETECTOR_INTERNAL_H

/*
 * Interface of KEDR functionality responsible for detection of
 * loading/unloading of the target module.
 */

#include <linux/module.h> /* 'struct module' definition */

struct kedr_target_module_notifier
{
    /* 
     * If set, this module will be preventing from unload while target
     * module is loaded.
     */
    struct module* mod;
    
    int (*on_target_load)(struct kedr_target_module_notifier* notifier,
        struct module* m);
    void (*on_target_unload)(struct kedr_target_module_notifier* notifier,
        struct module* m);

    /* May be implemented in the future */

    /* 
     * 'Hot' versions of the previous functions.
     * 
     * If notifier is initialized when target module is already loaded,
     * on_target_load_hot() will be called. If this functions return not 0,
     * initialization will fail.
     * 
     * If notifier is destroyed when target module is loaded,
     * on_target_unload_hot() will be called. If this functions return not 0,
     * destroing will fail.
     */
    
    //int (*on_target_load_hot)(struct kedr_target_module_notifier* notifier,
        //struct module* m);
    //int (*on_target_unload_hot)(struct kedr_target_module_notifier* notifier,
        //struct module* m);
        
};

/*
 * Initialize detector.
 * 
 * For simplification, only one notifier is available.
 * So, it is set at the initializing stage.
 * 
 * Before call of 'set_target_name' detector is not detect
 * loading and unloading of any module.
 */
int kedr_target_detector_init(struct kedr_target_module_notifier* notifier);

/*
 * Destroy detector.
 * 
 * Note: shouldn't be called when target module is loaded.
 */
void kedr_target_detector_destroy(void);

/*
 * Set name of the module, which loading and unloading should be detect.
 * 
 * Currently one may set name of the module only when module with previouse name is not loaded
 * and module with new name also is not loaded.
 */
int kedr_target_detector_set_target_name(const char* target_name);

/*
 * After this call detector is not detect
 * loading and unloading of any module.
 */
int kedr_target_detector_clear_target_name(void);

/*
 * Return name of the module we are watching for.
 * Result should be freed when no longer needed.
 * 
 * Return NULL if no module is currently being watched for.
 * 
 * Return ERR_PTR(error) on error.
 */
char* kedr_target_detector_get_target_name(void);

/*
 * Return not 0 if target module is currently loaded.
 */
int kedr_target_detector_is_target_loaded(void);

#endif /* KEDR_TARGET_DETECTOR_INTERNAL_H */