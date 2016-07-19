/*
 * The handler stubs for the events. These functions should do nothing
 * by themselves but they will contain the Ftrace placeholders if
 * CONFIG_FUNCTION_TRACER is set.
 * This will allow replacing them with the real handlers in runtime,
 * similar to how Livepatch does its job.
 * 
 * Compile this file and link to each binary you instrument with KEDR.
 */

#include <linux/stddef.h>	/* NULL */
/* ====================================================================== */

void *kedr_stub_fentry(void)
{
	return NULL;
}

void kedr_stub_fexit(void *lptr)
{
	(void)lptr;
}
/* ====================================================================== */

void kedr_stub_kmalloc_pre(unsigned long size, unsigned long gfp, void *lptr)
{
	(void)size;
	(void)gfp;
	(void)lptr;
}

void kedr_stub_kmalloc_post(unsigned long ret, void *lptr)
{
	(void)ret;
	(void)lptr;
}

void kedr_stub_kfree_pre(unsigned long ptr, void *lptr)
{
	(void)ptr;
	(void)lptr;
}

void kedr_stub_kfree_post(void *lptr)
{
	(void)lptr;
}

void kedr_stub_kmc_alloc_pre(unsigned long kmem_cache, unsigned gfp,
			     void *lptr)
{
	(void)kmem_cache;
	(void)gfp;
	(void)lptr;
}

void kedr_stub_kmc_alloc_post(unsigned long ret, void *lptr)
{
	(void)ret;
	(void)lptr;
}

void kedr_stub_kmc_free_pre(unsigned long kmem_cache, unsigned long ptr,
			    void *lptr)
{
	(void)kmem_cache;
	(void)ptr;
	(void)lptr;
}

void kedr_stub_kmc_free_post(void *lptr)
{
	(void)lptr;
}
