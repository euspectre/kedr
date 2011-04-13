/*********************************************************************
 * Module: <$module.name$>
 *********************************************************************/

<$header$>

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>

#include <kedr/core/kedr.h>
#include <kedr/util/stack_trace.h>

#include "memblock_info.h"
#include "klc_output.h"
#include "mbi_ops.h"

MODULE_AUTHOR("<$module.author$>");
MODULE_LICENSE("GPL");

/*********************************************************************
 * Parameters of the module
 *********************************************************************/
/* Default number of stack frames to output (at most) */
#define KEDR_STACK_DEPTH_DEFAULT 12

/* At most 'max_stack_entries' stack entries will be output for each 
 * suspicious allocation or deallocation. 
 * Should not exceed KEDR_MAX_FRAMES (see <kedr/util/stack_trace.h>).
 */
unsigned int stack_depth = KEDR_STACK_DEPTH_DEFAULT;
module_param(stack_depth, uint, S_IRUGO);

/*********************************************************************
 * The callbacks to be called after the target module has just been
 * loaded and, respectively, when it is about to unload.
 *********************************************************************/
static void
target_load_callback(struct module *target_module)
{
    BUG_ON(target_module == NULL);
    
    klc_output_clear();
    klc_print_target_module_info(target_module);
    return;
}

static void
target_unload_callback(struct module *target_module)
{
    BUG_ON(target_module == NULL);
    
    klc_flush_allocs();
    klc_flush_deallocs();
    klc_flush_stats();
    return;
}

/*********************************************************************
 * Replacement functions
 * 
 * [NB] Each deallocation should be processed in a replacement function
 * BEFORE calling the target function.
 * Each allocation should be processed AFTER the call to the target 
 * function.
 * This allows to avoid some problems on multiprocessor systems. As soon
 * as a memory block is freed, it may become the result of a new allocation
 * made by a thread on another processor. If a deallocation is processed 
 * after it has actually been done, a race condition could happen because 
 * another thread could break in during that gap.
 *********************************************************************/
<$block : join(\n)$>
/*********************************************************************/

/* Names and addresses of the functions of interest */
static struct kedr_pre_pair pre_pairs[] =
{
<$prePair: join()$>
    {
        .orig = NULL
    }
};

static struct kedr_post_pair post_pairs[] =
{
<$postPair: join()$>
    {
        .orig = NULL
    }
};



static struct kedr_payload payload = {
    .mod                    = THIS_MODULE,

    .pre_pairs              = pre_pairs,
    .post_pairs             = post_pairs,

    .target_load_callback   = target_load_callback,
    .target_unload_callback = target_unload_callback
};
/*********************************************************************/

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void
<$module.name$>_cleanup_module(void)
{
    kedr_payload_unregister(&payload);
    functions_support_unregister();
    klc_output_fini();

    KEDR_MSG("[<$module.name$>] Cleanup complete\n");
}

static int __init
<$module.name$>_init_module(void)
{
    int ret = 0;
    
    KEDR_MSG("[<$module.name$>] Initializing\n");
    
    if (stack_depth == 0 || stack_depth > KEDR_MAX_FRAMES) {
        printk(KERN_ERR "[<$module.name$>] "
            "Invalid value of 'stack_depth': %u (should be a positive "
            "integer not greater than %u)\n",
            stack_depth,
            KEDR_MAX_FRAMES
        );
        return -EINVAL;
    }
    
    klc_init_mbi_storage();
    
    ret = klc_output_init();
    if (ret != 0)
        return ret;
    
    ret = functions_support_register();
    if(ret != 0)
        goto fail_supp;

    ret = kedr_payload_register(&payload);
    if (ret != 0) 
        goto fail_reg;
  
    return 0;

fail_reg:
    functions_support_unregister();
fail_supp:
    klc_output_fini();
    return ret;
}

module_init(<$module.name$>_init_module);
module_exit(<$module.name$>_cleanup_module);
/*********************************************************************/
