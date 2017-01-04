#include <gcc-plugin.h>

#include "common_includes.h"
#include "i13n.h"

#include <map>
#include <string>

#include <cassert>
/* ====================================================================== */

/*
* kmalloc-like functions
* Arguments: (size_t size, gfp_t gfp);
* return value: void *.
*/
static struct kedr_function_class class_kmalloc = {
	.arg_pos = {1 /* size */, 0},
	.need_ret = true,
	.name_pre = "kedr_thunk_kmalloc_pre",
	.name_post = "kedr_thunk_kmalloc_post",
	.decl_pre = NULL_TREE,
	.decl_post = NULL_TREE
};

/*
 * kfree-like functions
 * Arguments: (void *);
 * return value: none.
 */
static kedr_function_class class_kfree = {
	.arg_pos = {1 /* ptr */, 0},
	.need_ret = false,
	.name_pre = "kedr_thunk_kfree_pre",
	.name_post = "kedr_thunk_kfree_post",
	.decl_pre = NULL_TREE,
	.decl_post = NULL_TREE
};

/*
 * kmem_cache_alloc-like functions
 * Arguments: (struct kmem_cache *, gfp_t);
 * return value: void *.
 */
static kedr_function_class class_kmc_alloc = {
	.arg_pos = {1 /* kmem_cache */, 0},
	.need_ret = true,
	.name_pre = "kedr_thunk_kmc_alloc_pre",
	.name_post = "kedr_thunk_kmc_alloc_post",
	.decl_pre = NULL_TREE,
	.decl_post = NULL_TREE
};

/*
 * kmem_cache_free
 * Arguments: (struct kmem_cache *, void *);
 * return value: none.
 */
static kedr_function_class class_kmc_free = {
	.arg_pos = {1 /* kmem_cache */, 2 /* ptr */, 0},
	.need_ret = false,
	.name_pre = "kedr_thunk_kmc_free_pre",
	.name_post = "kedr_thunk_kmc_free_post",
	.decl_pre = NULL_TREE,
	.decl_post = NULL_TREE
};
/* ====================================================================== */

namespace {

class function_matcher
{
private:
	typedef std::map<std::string, kedr_function_class *> TClassMap;
public:
	function_matcher();

	kedr_function_class *
	get_class_by_fname(const std::string & fname) {
		TClassMap::iterator it;

		it = classes.find(fname);
		if (it == classes.end())
			return NULL;

		return it->second;
	}

private:
	/* Populate the map with {fname => function_class} pairs. */
	void populate_map();

private:
	TClassMap classes;
};

function_matcher::function_matcher()
{
	populate_map();
}

void
function_matcher::populate_map()
{
	/* kmalloc */
	classes["__kmalloc"] = &class_kmalloc;
	classes["kmalloc_order"] = &class_kmalloc;
	classes["kmalloc_order_trace"] = &class_kmalloc;
	classes["alloc_pages_exact"] = &class_kmalloc;

	/* kmem_cache_alloc */
	classes["kmem_cache_alloc"] = &class_kmc_alloc;
	classes["kmem_cache_alloc_node"] = &class_kmc_alloc;
	classes["kmem_cache_alloc_trace"] = &class_kmc_alloc;
	classes["kmem_cache_alloc_node_trace"] = &class_kmc_alloc;

	/* kfree */
	classes["kfree"] = &class_kfree;
	classes["kzfree"] = &class_kfree;
	classes["free_pages_exact"] = &class_kfree;
	classes["vfree"] = &class_kfree;
	classes["kvfree"] = &class_kfree;

	/* kmem_cache_free */
	classes["kmem_cache_free"] = &class_kmc_free;
}
/* ====================================================================== */
} /* end of anon namespace */

/* This makes sure the matcher is initialized before everything else. */
static function_matcher fm;

static tree make_decl_pre(const kedr_function_class *fc)
{
	tree arg_types[KEDR_NR_ARGS + 1];
	int i;

	for (i = 0; (i <= KEDR_NR_ARGS) && (fc->arg_pos[i]); ++i) {
		assert(i < KEDR_NR_ARGS);
		arg_types[i] = long_unsigned_type_node;
	}
	arg_types[i] = ptr_type_node; /* void *lptr */

	tree fntype = build_function_type_array(
		void_type_node, i + 1, arg_types);
	tree decl = build_fn_decl(fc->name_pre, fntype);

	assert(decl != NULL_TREE);
	kedr_set_fndecl_properties(decl);
	return decl;
}

static tree make_decl_post(const kedr_function_class *fc)
{
	int i = 0;
	tree arg_types[2];

	if (fc->need_ret) {
		arg_types[0] = long_unsigned_type_node;
		i = 1;
	}
	arg_types[i] = ptr_type_node; /* void *lptr */

	tree fntype = build_function_type_array(
		void_type_node, i + 1, arg_types);
	tree decl = build_fn_decl(fc->name_post, fntype);

	assert(decl != NULL_TREE);
	kedr_set_fndecl_properties(decl);
	return decl;
}

const kedr_function_class *kedr_get_class_by_fname(
	const char *fname)
{
	struct kedr_function_class *fc = fm.get_class_by_fname(std::string(fname));

	if (!fc)
		return NULL;

	if (fc->decl_pre == NULL_TREE || fc->decl_post == NULL_TREE) {
		/* Either both or none should be set. */
		assert(fc->decl_pre == NULL_TREE &&
		       fc->decl_post == NULL_TREE);

		fc->decl_pre = make_decl_pre(fc);
		fc->decl_post = make_decl_post(fc);
	}

	return fc;
}
/* ====================================================================== */
