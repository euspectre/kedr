// SPDX-License-Identifier: GPL-2.0
/*
 * This test module generates different kinds of events ("alloc", etc.) in
 * different situations. The tests can use this to check if KEDR captures
 * the needed events and processes them correctly.
 *
 * It is not needed to try calling all the functions in the tests here,
 * esp. implementation-specific ones, like kmalloc_order_trace(), for
 * example. Rather, call what the kernel and the modules are likely to call:
 * kmalloc, vmalloc, kstrdup, etc.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/topology.h>
#include <linux/vmalloc.h>
#include <linux/rcupdate.h>

MODULE_AUTHOR("Evgenii Shatokhin");
MODULE_LICENSE("GPL");

#define MAX_TEST_ID_LEN 64

#define MSG_PREFIX "test_events: "

static unsigned long var = 0x42;
module_param(var, ulong, S_IRUGO);

static struct dentry *debugfs_dir;
static struct dentry *test_id_file;

static DEFINE_MUTEX(test_mutex);

struct test_func_item
{
	const char *id;

	 /*
	  * Returns 0 on success, -ERR on failure.
	  * 'buf' and 'count' are for *dup_user*() functions.
	  */
	int (*func) (const char __user *buf, size_t count);
};

/* kmalloc/kfree for a builtin-constant size < KMALLOC_MAX_CACHE_SIZE */
static int test_kmalloc_const_small(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc(64, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

/* kmalloc/kfree for a builtin-constant size > KMALLOC_MAX_CACHE_SIZE */
static int test_kmalloc_const_large(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc(2 * PAGE_SIZE, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

/* kmalloc/kfree for a non-constant size < KMALLOC_MAX_CACHE_SIZE */
static int test_kmalloc_var_small(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc(var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

/* kmalloc/kfree for a builtin-constant size > KMALLOC_MAX_CACHE_SIZE */
static int test_kmalloc_var_large(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc(var * PAGE_SIZE / 0x21, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

/* same tests but for kmalloc_node/kfree */
static int test_kmalloc_node_const_small(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc_node(64, GFP_KERNEL, numa_node_id());
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_kmalloc_node_const_large(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc_node(2 * PAGE_SIZE, GFP_KERNEL, numa_node_id());
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_kmalloc_node_var_small(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc_node(var, GFP_KERNEL, numa_node_id());
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_kmalloc_node_var_large(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc_node(var * PAGE_SIZE / 0x21, GFP_KERNEL, numa_node_id());
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_kvmalloc(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kvmalloc(var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kvfree(p);
	return 0;
}

static int test_kvmalloc_node(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kvmalloc_node(var, GFP_KERNEL, numa_node_id());
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kvfree(p);
	return 0;
}

static int test_kzfree(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kzalloc(var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kzfree(p);
	return 0;
}

static int test_kmem_cache_alloc(const char __user *buf, size_t count)
{
	void *p;
	struct kmem_cache* mc;
	int ret = 0;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	mc = kmem_cache_create("kedr_test_kmem_cache", 32, 32, 0, NULL);
	if (!mc) {
		pr_info(MSG_PREFIX "%s: failed to create memory cache.\n",
			__func__);
		return -ENOMEM;
	}

	p = kmem_cache_alloc(mc, GFP_KERNEL);
	if (p) {
		kmem_cache_free(mc, p);
	}
	else {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		ret = -ENOMEM;
	}

	kmem_cache_destroy(mc);
	return ret;
}

/* vmalloc */
static int test_vmalloc(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = vmalloc(var);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	vfree(p);
	return 0;
}

static int test_vzalloc(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = vzalloc(var);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	vfree(p);
	return 0;
}

static int test_vmalloc_node(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = vmalloc_node(var, numa_node_id());
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	vfree(p);
	return 0;
}

static int test_vzalloc_node(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = vzalloc_node(var, numa_node_id());
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	vfree(p);
	return 0;
}

static int test_vmalloc_32(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = vmalloc_32(var);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	vfree(p);
	return 0;
}

static int test_vmalloc_user(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = vmalloc_user(var);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	vfree(p);
	return 0;
}

/*
 * krealloc
 * The following cases are checked here:
 * - original ptr is non-NULL and new size is greater than the old size;
 * - original ptr is non-NULL and new size is less than the old size;
 * - original ptr is NULL and new size is non-zero;
 * - original ptr is non-NULL and new size is 0.
 */
static int test_krealloc_enlarge(const char __user *buf, size_t count)
{
	void *p, *new_p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kmalloc(var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}

	new_p = krealloc(p, 4 * var, GFP_KERNEL);
	if (new_p) {
		kfree(new_p);
	}
	else {
		kfree(p);
		pr_info(MSG_PREFIX "%s: failed to reallocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	return 0;
}

static int test_krealloc_shrink(const char __user *buf, size_t count)
{
	void *p, *new_p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kmalloc(4 * var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}

	new_p = krealloc(p, var, GFP_KERNEL);
	if (new_p) {
		kfree(new_p);
	}
	else {
		kfree(p);
		pr_info(MSG_PREFIX "%s: failed to reallocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	return 0;
}

static int test_krealloc_alloc(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = krealloc(NULL, var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_krealloc_free(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kmalloc(var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}

	/* frees 'p', returns ZERO_SIZE_PTR */
	p = krealloc(p, 0, GFP_KERNEL);
	return 0;
}

/*
 * __krealloc
 * The following cases are checked here:
 * - original ptr is non-NULL and new size is greater than the old size;
 * - original ptr is non-NULL and new size is less than the old size;
 * - original ptr is NULL and new size is non-zero.
 */
static int test___krealloc_enlarge(const char __user *buf, size_t count)
{
	void *p, *new_p;
	int ret = 0;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kmalloc(var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}

	new_p = __krealloc(p, 4 * var, GFP_KERNEL);
	if (!new_p) {
		pr_info(MSG_PREFIX "%s: failed to reallocate memory.\n",
			__func__);
		ret = -ENOMEM;
	}

	if (new_p != p)
		kfree(new_p);
	kfree(p);
	return ret;
}

static int test___krealloc_shrink(const char __user *buf, size_t count)
{
	void *p, *new_p;
	int ret = 0;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kmalloc(4 * var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}

	new_p = __krealloc(p, var, GFP_KERNEL);
	if (!new_p) {
		pr_info(MSG_PREFIX "%s: failed to reallocate memory.\n",
			__func__);
		ret = -ENOMEM;
	}

	if (new_p != p)
		kfree(new_p);
	kfree(p);
	return ret;
}

static int test___krealloc_alloc(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = __krealloc(NULL, var, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_alloc_pages(const char __user *buf, size_t count)
{
	struct page *page;
	unsigned int order = 1;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	page = alloc_pages(GFP_KERNEL, order);
	if (!page) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	__free_pages(page, order);
	return 0;
}

static int test___get_free_pages(const char __user *buf, size_t count)
{
	unsigned long addr;
	unsigned int order = 1;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	addr = __get_free_pages(GFP_KERNEL, order);
	if (!addr) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	free_pages(addr, order);
	return 0;
}

static int test_get_zeroed_page(const char __user *buf, size_t count)
{
	unsigned long addr;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	addr = get_zeroed_page(GFP_KERNEL);
	if (!addr) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	free_page(addr);
	return 0;
}

static int test_kmemdup(const char __user *buf, size_t count)
{
	void *p;
	const char *str = "Something";

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kmemdup(str, 4, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_kstrdup(const char __user *buf, size_t count)
{
	void *p;
	const char *str = "Something";

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kstrdup(str, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_kstrndup(const char __user *buf, size_t count)
{
	void *p;
	const char *str = "Something";

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kstrndup(str, 4, GFP_KERNEL);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static int test_memdup_user(const char __user *buf, size_t count)
{
	void *p;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = memdup_user(buf, count);
	if (IS_ERR(p)) {
		pr_info(MSG_PREFIX "%s: memdup_user() failed.\n",
			__func__);
		return PTR_ERR(p);
	}
	kfree(p);
	return 0;
}

/* vmemdup_user() first appeared in kernel 4.16 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
static int test_vmemdup_user(const char __user *buf, size_t count)
{
	void *p;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = vmemdup_user(buf, count);
	if (IS_ERR(p)) {
		pr_info(MSG_PREFIX "%s: vmemdup_user() failed.\n",
			__func__);
		return PTR_ERR(p);
	}
	kvfree(p);
	return 0;
}
#else
/* This test will always fail to make it easy to notice. */
static int test_vmemdup_user(const char __user *buf, size_t count)
{
	(void)buf;
	(void)count;
	return -EINVAL;
}
#endif

static int test_memdup_user_nul(const char __user *buf, size_t count)
{
	void *p;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = memdup_user_nul(buf, count);
	if (IS_ERR(p)) {
		pr_info(MSG_PREFIX "%s: memdup_user_nul() failed.\n",
			__func__);
		return PTR_ERR(p);
	}
	kfree(p);
	return 0;
}

static int test_strndup_user(const char __user *buf, size_t count)
{
	void *p;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = strndup_user(buf, 32);
	if (IS_ERR(p)) {
		pr_info(MSG_PREFIX "%s: strndup_user() failed.\n",
			__func__);
		return PTR_ERR(p);
	}
	kfree(p);
	return 0;
}

static int test_kasprintf(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = kasprintf(GFP_KERNEL, "Count is %zu, func is %p.",
		      count, test_kasprintf);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

static noinline char *my_kasprintf_for_test(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return p;
}

static int test_kvasprintf(const char __user *buf, size_t count)
{
	void *p;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	p = my_kasprintf_for_test(GFP_KERNEL, "Count is %zu, func is %p.",
				  count, test_kvasprintf);
	if (!p) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}
	kfree(p);
	return 0;
}

struct my_test_struct
{
	void *something;
	struct rcu_head rcu;
};

static int test_kfree_rcu(const char __user *buf, size_t count)
{
	struct my_test_struct *obj;

	(void)buf;
	(void)count;

	pr_info(MSG_PREFIX "%s.\n", __func__);
	obj = kmalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		pr_info(MSG_PREFIX "%s: failed to allocate memory.\n",
			__func__);
		return -ENOMEM;
	}

	kfree_rcu(obj, rcu);
	/*
	 * rcu_barrier() is not needed here at the moment. KEDR reports
	 * the "free" event for kfree_rcu() immediately, so we don't have
	 * to wait for kfree() to run after the grace period.
	 *
	 * kfree_rcu() does not need the callbacks from this module either,
	 * so the module can be safely unloaded even before kfree() runs.
	 */
	return 0;
}

static struct test_func_item tests[] = {
	{
		.id = "kmalloc.01",
		.func = test_kmalloc_const_small,
	},
	{
		.id = "kmalloc.02",
		.func = test_kmalloc_const_large,
	},
	{
		.id = "kmalloc.03",
		.func = test_kmalloc_var_small,
	},
	{
		.id = "kmalloc.04",
		.func = test_kmalloc_var_large,
	},
	{
		.id = "kmalloc_node.01",
		.func = test_kmalloc_node_const_small,
	},
	{
		.id = "kmalloc_node.02",
		.func = test_kmalloc_node_const_large,
	},
	{
		.id = "kmalloc_node.03",
		.func = test_kmalloc_node_var_small,
	},
	{
		.id = "kmalloc_node.04",
		.func = test_kmalloc_node_var_large,
	},
	{
		.id = "kvmalloc.01",
		.func = test_kvmalloc,
	},
	{
		.id = "kvmalloc.02",
		.func = test_kvmalloc_node,
	},
	{
		.id = "kzfree.01",
		.func = test_kzfree,
	},
	{
		.id = "kmem_cache_alloc.01",
		.func = test_kmem_cache_alloc,
	},
	{
		.id = "vmalloc.01",
		.func = test_vmalloc,
	},
	{
		.id = "vzalloc.01",
		.func = test_vzalloc,
	},
	{
		.id = "vmalloc_node.01",
		.func = test_vmalloc_node,
	},
	{
		.id = "vzalloc_node.01",
		.func = test_vzalloc_node,
	},
	{
		.id = "vmalloc_32.01",
		.func = test_vmalloc_32,
	},
	{
		.id = "vmalloc_user.01",
		.func = test_vmalloc_user,
	},
	{
		.id = "krealloc.01",
		.func = test_krealloc_enlarge,
	},
	{
		.id = "krealloc.02",
		.func = test_krealloc_shrink,
	},
	{
		.id = "krealloc.03",
		.func = test_krealloc_alloc,
	},
	{
		.id = "krealloc.04",
		.func = test_krealloc_free,
	},
	{
		.id = "__krealloc.01",
		.func = test___krealloc_enlarge,
	},
	{
		.id = "__krealloc.02",
		.func = test___krealloc_shrink,
	},
	{
		.id = "__krealloc.03",
		.func = test___krealloc_alloc,
	},
	{
		.id = "alloc_pages.01",
		.func = test_alloc_pages,
	},
	{
		.id = "__get_free_pages.01",
		.func = test___get_free_pages,
	},
	{
		.id = "get_zeroed_page.01",
		.func = test_get_zeroed_page,
	},
	{
		.id = "kmemdup.01",
		.func = test_kmemdup,
	},
	{
		.id = "kstrdup.01",
		.func = test_kstrdup,
	},
	{
		.id = "kstrndup.01",
		.func = test_kstrndup,
	},
	{
		.id = "memdup_user.01",
		.func = test_memdup_user,
	},
	{
		.id = "vmemdup_user.01",
		.func = test_vmemdup_user,
	},
	{
		.id = "strndup_user.01",
		.func = test_strndup_user,
	},
	{
		.id = "memdup_user_nul.01",
		.func = test_memdup_user_nul,
	},
	{
		.id = "kasprintf.01",
		.func = test_kasprintf,
	},
	{
		.id = "kvasprintf.01",
		.func = test_kvasprintf,
	},
	{
		.id = "kfree_rcu.01",
		.func = test_kfree_rcu,
	},
};

static int run_test_by_id(const char *test_id, const char __user *buf,
			  size_t count)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tests); ++i) {
		if (!strcmp(test_id, tests[i].id))
			return tests[i].func(buf, count);
	}

	pr_info(MSG_PREFIX "%s: unknown test \"%s\".\n", __func__, test_id);
	return -EINVAL;
}

static ssize_t test_id_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *f_pos)
{
	int ret;
	char test_id[MAX_TEST_ID_LEN];

	if (*f_pos != 0 || count == 0 || count >= MAX_TEST_ID_LEN)
		return -EINVAL;

	if (copy_from_user(test_id, buf, count) != 0)
		return -EFAULT;

	test_id[count] = 0;
	if (test_id[count - 1] == '\n')
		test_id[count - 1] = 0;

	mutex_lock(&test_mutex);
	ret = run_test_by_id(test_id, buf, count);
	if (ret)
		goto out;

	*f_pos += count;
	ret = count;
out:
	mutex_unlock(&test_mutex);
	return ret;
}

struct file_operations test_id_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = test_id_write,
	.llseek = no_llseek,
};

static int __init test_init_module(void)
{
	int ret = -EINVAL;

	debugfs_dir = debugfs_create_dir("kedr_test_events", NULL);
	if (!debugfs_dir) {
		pr_warning(MSG_PREFIX
			   "Failed to create a directory in debugfs\n");
		goto err;
	}

	test_id_file = debugfs_create_file("test_id", (S_IWUSR|S_IWGRP),
					   debugfs_dir, NULL, &test_id_ops);
	if (!test_id_file) {
		pr_warning(MSG_PREFIX
			   "Failed to create a file in debugfs\n");
		goto err_removedir;
	}
	return 0;

err_removedir:
	debugfs_remove(debugfs_dir);
err:
	return ret;
}

static void __exit test_exit_module(void)
{
	debugfs_remove(test_id_file);
	debugfs_remove(debugfs_dir);
	return;
}

module_init(test_init_module);
module_exit(test_exit_module);
