#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

#include <kedr/base/common.h>

#include "counters.h"

/* ================================================================ */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ================================================================ */

// TODO

/* ================================================================ */
static int __init
counters_init(void)
{
/*  int result = kedr_payload_register(&counters_payload);
	if (result)
    {
        printk(KERN_ERR "Failed to register payload module "counters".\n");
        return result;
    }
*/
    return 0;
}

static void
counters_exit(void)
{
	/*kedr_payload_unregister(&counters_payload);*/
	return;
}

module_init(counters_init);
module_exit(counters_exit);
/* ================================================================ */
