#ifndef KEDR_INTERNAL_H
#define KEDR_INTERNAL_H

#define kedr_err(format_str, ...) pr_err("KEDR: " format_str, __VA_ARGS__)
#define kedr_err0(message) pr_err("KEDR: %s", message)

/*
 * Current implementation of kernel module loading is bugged. If:
 * 
 * 0) lockdep is enabled in the kernel
 * 1) module param is set via 'insmod' parameters
 * 2) parameter's 'set' callback uses lock/mutex defined in the module
 * 3) parameter's 'set' callback returns error
 * 
 * Then information about the lock/mutex is not correctly freed by
 * lockdep, and lockdep will crash on next attempt to process new lock.
 * 
 * Alternative to mutex is using kparam_block_sysfs_write(), but this
 * functionality is inacceptable until 2.6.36.
 * 
 * Next function return non-zero if module's locks may be safetly used
 * in parameter's 'set' callback.
 * When function returns zero, locking may be omitted.
 */
static inline int param_set_can_use_lock(void)
{
    /*
     * module->holders_dir is set after parsing initial module parameters.
     * This field is exists since 2.6.21, so can be safetly used.
     */
    return !!THIS_MODULE->holders_dir;
}


#endif /* KEDR_INTERNAL_H */