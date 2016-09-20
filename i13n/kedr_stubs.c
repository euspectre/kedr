/*
 * The thunk functions for the events. The purpose of these functions is
 * to prepare the information about the event, store it in an instance of
 * struct kedr_local and pass to the handler stub.
 *
 * The stubs do nothing by themselves but they will contain the Ftrace
 * placeholders if CONFIG_FUNCTION_TRACER is set.
 * This will allow replacing them with the real handlers in runtime,
 * similar to how Livepatch does its job.
 *
 * Compile this file and link to each binary you instrument with KEDR.
 * Do not forget to add the path to the KEDR header files to the compiler
 * options.
 */

#include <linux/compiler.h>	/* notrace, ... */
#include <linux/stddef.h>	/* NULL */
#include <linux/slab.h>

#include <linux/kedr_local.h>	/* struct kedr_local */
/* ====================================================================== */

/*
 * If no real handlers are attached, the data will be stored in this object
 * and will be ignored because the object is not marked as valid.
 * This is just to return something meaningful from kedr_stub_fentry() and
 * to avoid checking lptr for NULL in the thunks.
 */
static struct kedr_local _local;
/* ====================================================================== */

void __used *kedr_stub_fentry(void)
{
	return &_local;
}

void __used kedr_stub_fexit(struct kedr_local *local)
{
	(void)local;
}
/* ====================================================================== */

/*
 * Hander stubs
 *
 * alloc_* - memory allocation event,
 * free_* - memory deallocation event.
 *
 * What is guaranteed:
 *
 * for all handlers:
 * - local is the address of an existing kedr_local instance, never NULL;
 * - local->pc is meaningful.
 * - If a thunk for a pre-handler sets 'addr', 'size', 'event' or 'pc'
 *   field of kedr_local instance, these fields will not change until
 *   the thunk for a post-handler runs. This also requires that the handlers
 *   do not change these values as well.
 *
 * alloc_pre:
 * - local->size is the requested size of the memory block (> 0).
 *
 * alloc_post:
 * - called only if the allocation succeeds;
 * - local->size is the requested size of the memory block (unless
 *   alloc_pre() has changed it);
 * - local->addr is the address of the allocated memory block.
 *
 * free_pre:
 * - local->addr is the address of the memory block to be freed (non-NULL,
 *   not a ZERO_SIZE_PTR either).
 *
 * free_post:
 * - local->addr is the same as for free_pre() unless the latter has changed
 *   it.
 */
void __used kedr_stub_alloc_pre(struct kedr_local *local)
{
	(void)local;
}

void __used kedr_stub_alloc_post(struct kedr_local *local)
{
	(void)local;
}

void __used kedr_stub_free_pre(struct kedr_local *local)
{
	(void)local;
}

void __used kedr_stub_free_post(struct kedr_local *local)
{
	(void)local;
}
/* ====================================================================== */

void notrace kedr_thunk_kmalloc_pre(unsigned long size,
				    struct kedr_local *local)
{
	/*
	 * Set these fields even if 'size' is 0: the thunk for the post
	 * handler may need them to decide if that handler should be called.
	 */
	local->pc = (unsigned long)__builtin_return_address(0);
	local->size = size;

	if (size == 0)
		return;

	kedr_stub_alloc_pre(local);
}

void notrace kedr_thunk_kmalloc_post(unsigned long ret,
				     struct kedr_local *local)
{
	if (local->size == 0 || ZERO_OR_NULL_PTR((void *)ret))
		return;

	/* local->pc must have been set by the thunk for the pre-handler. */
	local->addr = ret;
	kedr_stub_alloc_post(local);
}

void notrace kedr_thunk_kfree_pre(unsigned long ptr,
				  struct kedr_local *local)
{
	local->pc = (unsigned long)__builtin_return_address(0);
	local->addr = ptr;

	if (ZERO_OR_NULL_PTR((void *)ptr))
		return;

	kedr_stub_free_pre(local);
}

void notrace kedr_thunk_kfree_post(struct kedr_local *local)
{
	if (ZERO_OR_NULL_PTR((void *)local->addr))
		return;

	kedr_stub_free_post(local);
}

void notrace kedr_thunk_kmc_alloc_pre(unsigned long kmem_cache,
				      struct kedr_local *local)
{
	struct kmem_cache *kmc = (struct kmem_cache *)kmem_cache;

	local->pc = (unsigned long)__builtin_return_address(0);
	if (kmc)
		local->size = (unsigned long)kmem_cache_size(kmc);
	else
		local->size = 0;

	if (local->size == 0)
		return;

	kedr_stub_alloc_pre(local);
}

void notrace kedr_thunk_kmc_alloc_post(unsigned long ret,
				       struct kedr_local *local)
{
	if (local->size == 0 || ZERO_OR_NULL_PTR((void *)ret))
		return;

	local->addr = ret;
	kedr_stub_alloc_post(local);
}

void notrace kedr_thunk_kmc_free_pre(unsigned long kmem_cache,
				     unsigned long ptr,
				     struct kedr_local *local)
{
	(void)kmem_cache;

	local->pc = (unsigned long)__builtin_return_address(0);
	local->addr = ptr;

	if (ZERO_OR_NULL_PTR((void *)ptr))
		return;

	kedr_stub_free_pre(local);
}

void notrace kedr_thunk_kmc_free_post(struct kedr_local *local)
{
	if (ZERO_OR_NULL_PTR((void *)local->addr))
		return;

	kedr_stub_free_post(local);
}
