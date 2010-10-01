#include <linux/module.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>  /* kmalloc replacement*/

#include <kedr/fault_simulation/fault_simulation.h>
#include <kedr/base/common.h>

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

static const char* point_kmalloc_name = "sample_point_kmalloc";
//According to convensions of 'format_string' of fault simulation
struct point_data
{
    size_t size;
    gfp_t flags;
};

struct kedr_simulation_point* point_kmalloc;

//
static void* repl___kmalloc(size_t size, gfp_t flags)
{
    struct point_data point_data;
    point_data.size = size;
    point_data.flags = flags;
    
    return kedr_fsim_point_simulate(point_kmalloc, &point_data) ?
        NULL : __kmalloc(size, flags);
}

static void* p___kmalloc = (void*)__kmalloc;
static void* p_repl___kmalloc = (void*)repl___kmalloc;

struct kedr_payload fsim_payload_kmalloc =
{
    .mod = THIS_MODULE,//
    .repl_table =
    {
        .orig_addrs = &p___kmalloc,//
        .repl_addrs = &p_repl___kmalloc,//
        .num_addrs = 1,
    },
};

static int __init
fsim_payload_kmalloc_init(void)
{
    point_kmalloc = kedr_fsim_point_register(point_kmalloc_name,
        "size_t,gfp_flags", THIS_MODULE);
    
    if(point_kmalloc == NULL)
    {
        printk(KERN_ERR "Cannot register simulation point.\n");
        return 1;
    }
    if(kedr_payload_register(&fsim_payload_kmalloc))
    {
        printk(KERN_ERR "Cannot register payload.\n");
        kedr_fsim_point_unregister(point_kmalloc);
        return 1;
    }

    return 0;
}

static void
fsim_payload_kmalloc_exit(void)
{
	kedr_payload_unregister(&fsim_payload_kmalloc);
	kedr_fsim_point_unregister(point_kmalloc);
	return;
}

module_init(fsim_payload_kmalloc_init);
module_exit(fsim_payload_kmalloc_exit);