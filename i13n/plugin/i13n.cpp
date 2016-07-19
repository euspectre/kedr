/* Some of the functions here are based on the implementation of
 * ThreadSanitizer from GCC 5.3 (gcc/tsan.c). */

#include <assert.h>
#include <string.h>
#include <gcc-plugin.h>
#include <plugin-version.h>

#include "config.h"
#include "common_includes.h"
#include "i13n.h"

//<>
#include <stdio.h> // for debugging
//<>
/* ====================================================================== */

/* Use this to mark the symbols to be exported from this plugin. The
 * remaining symbols will not be visible from outside of this plugin even
 * if they are not static (-fvisibility=hidden GCC option is used to achieve
 * this). */
#define PLUGIN_EXPORT __attribute__ ((visibility("default")))
/* ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* This symbol is needed for GCC to know the plugin is GPL-compatible. */
PLUGIN_EXPORT int plugin_is_GPL_compatible;

/* Plugin initialization function, the only function to be exported */
PLUGIN_EXPORT int
plugin_init(struct plugin_name_args *plugin_info,
	    struct plugin_gcc_version *version);

#ifdef __cplusplus
}
#endif
/* ====================================================================== */

void
kedr_set_fndecl_properties(tree fndecl)
{
	DECL_EXTERNAL(fndecl) = 1;
	DECL_ARTIFICIAL(fndecl) = 1;
}

static void
instrument_fentry(tree &ls_ptr)
{
	static tree fentry_decl = NULL_TREE;
	basic_block on_entry;
	gimple_stmt_iterator gsi;
	gimple g;
	gimple_seq seq = NULL;

	if (fentry_decl == NULL_TREE) {
		tree fntype = build_function_type_list(
			ptr_type_node, void_type_node, NULL_TREE);
		fentry_decl = build_fn_decl("kedr_stub_fentry", fntype);
		kedr_set_fndecl_properties(fentry_decl);
	}

	on_entry = single_succ(ENTRY_BLOCK_PTR_FOR_FN(cfun));
	gsi = gsi_start_bb(on_entry);

	g = gimple_build_call(fentry_decl, 0);
	gimple_call_set_lhs(g, ls_ptr);
	gimple_set_location(g, cfun->function_start_locus);
	gimple_seq_add_stmt(&seq, g);
	gsi_insert_seq_before(&gsi, seq, GSI_SAME_STMT);
}

static void
instrument_fexit(tree &ls_ptr)
{
	static tree fexit_decl = NULL_TREE;
	basic_block at_exit;
	edge e;
	edge_iterator ei;

	if (fexit_decl == NULL_TREE) {
		tree fntype = build_function_type_list(
			void_type_node, ptr_type_node, NULL_TREE);
		fexit_decl = build_fn_decl("kedr_stub_fexit", fntype);
		kedr_set_fndecl_properties(fexit_decl);
	}

	/*
	 * We need to place the call to our exit handler right before the 
	 * last operation in the function (e.g. "return" of some kind).
	 * Note that this is NOT right before the end node of the function. 
	 * 
	 * Let us find the exits from the function similar to how TSan does
	 * that in GCC).
	 */
	at_exit = EXIT_BLOCK_PTR_FOR_FN(cfun);
	FOR_EACH_EDGE(e, ei, at_exit->preds) {
		location_t loc;
		gimple_stmt_iterator gsi;
		gimple stmt;
		gimple g;

		gsi = gsi_last_bb(e->src);
		stmt = gsi_stmt(gsi);
		/* Sanity check, just in case */
		gcc_assert(gimple_code(stmt) == GIMPLE_RETURN || 
			   gimple_call_builtin_p(stmt, BUILT_IN_RETURN));

		loc = gimple_location(stmt);
		g = gimple_build_call(fexit_decl, 1, ls_ptr);
		gimple_set_location(g, loc);
		gsi_insert_before(&gsi, g, GSI_SAME_STMT);
	}
}

/* 
 * Prepare the argument of the handler from the given argument or return
 * value of the target function. Add type conversions where necessary.
 * 
 * If the value is neither an integer nor a pointer, take its address and
 * use it instead.
 */
static tree prepare_handler_arg(tree arg, gimple_seq *seq)
{
	gimple g;
	tree src_type = TREE_TYPE(arg);

	if (!POINTER_TYPE_P(src_type) && !INTEGRAL_TYPE_P(src_type)) {
		tree var  = make_ssa_name(ptr_type_node);
		g = gimple_build_assign(
			var, build_fold_addr_expr(unshare_expr(arg)));
		gimple_seq_add_stmt(seq, g);
		arg = var;
		src_type = ptr_type_node;
	}

	if (!useless_type_conversion_p(long_unsigned_type_node, src_type)) {
		tree var = make_ssa_name(long_unsigned_type_node);
		g = gimple_build_assign(var, NOP_EXPR, arg);
		gimple_seq_add_stmt(seq, g);
		arg = var;
	}

	return arg;
}

/* 
 * Add the handlers for the function calls of interest.
 * 'ls' - pointer to the local storage.
 * 
 * Returns true if it has instrumented the call, false otherwise.
 */
static bool
instrument_function_call(gimple_stmt_iterator *gsi, tree &ls_ptr)
{
	gimple stmt = gsi_stmt(*gsi);
	gimple_seq seq;
	gimple g;

	tree fndecl = gimple_call_fndecl(stmt);
	if (!fndecl)
		return false; /* Indirect call, nothing to do. */

	const char *name = IDENTIFIER_POINTER(DECL_NAME(fndecl));
	const struct kedr_function_class *fc = kedr_get_class_by_fname(name);
	if (!fc) /* No class is defined for this function, skip it. */
		return false;

	//<>
	fprintf(stderr, "[DBG] Direct call to %s\n", name);
	//<>

	/* Prepare the arguments and insert a call to the pre-handler. */
	seq = NULL;
	vec<tree> args_pre = vNULL;
	for (int i = 0; fc->arg_pos[i]; ++i) {
		int n = (int)fc->arg_pos[i] - 1;
		tree arg = gimple_call_arg(stmt, n);
		arg = prepare_handler_arg(arg, &seq);
		args_pre.safe_push(arg);
	}
	
	args_pre.safe_push(ls_ptr);
	g = gimple_build_call_vec(fc->decl_pre, args_pre);
	gimple_seq_add_stmt(&seq, g);
	gsi_insert_seq_before(gsi, seq, GSI_SAME_STMT);

	/* Prepare the call to the post-handler. */
	seq = NULL;
	vec<tree> args_post = vNULL;
	if (fc->need_ret) {
		tree ret = gimple_call_lhs(stmt);
		tree ret_type = gimple_call_return_type(as_a<gcall *>(stmt));

		assert(!types_compatible_p(ret_type, void_type_node));

		/* 
		 * If the return value is ignored, store it in a temporary,
		 * the handler might still need it.
		 */
		if (!ret) {
			ret = create_tmp_var(ret_type);
			mark_addressable(ret);
			gimple_call_set_lhs(stmt, ret);
		}

		tree arg = prepare_handler_arg(ret, &seq);
		args_post.safe_push(arg);
	}

	args_post.safe_push(ls_ptr);
	g = gimple_build_call_vec(fc->decl_post, args_post);
	gimple_seq_add_stmt(&seq, g);
	gsi_insert_seq_after(gsi, seq, GSI_CONTINUE_LINKING);

	return true;
}

static bool
instrument_gimple(gimple_stmt_iterator *gsi, tree &ls_ptr)
{
	gimple stmt;
	stmt = gsi_stmt(*gsi);
	bool need_ls = false;

	if (is_gimple_call(stmt))
		need_ls |= instrument_function_call(gsi, ls_ptr);

	return need_ls;
}

/* Process the body of the function. */
static bool
instrument_function(tree &ls_ptr)
{
	basic_block bb;
	gimple_stmt_iterator gsi;
	bool need_ls = false;

	FOR_EACH_BB_FN (bb, cfun) {
		for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi);
		     gsi_next(&gsi)) {
			need_ls |= instrument_gimple(&gsi, ls_ptr);
		}
	}
	return need_ls;
}

/* The main function of "kedr-i13n-calls" pass.
 * Called for each function to be processed. */
static unsigned int
execute_pass(function */*f*/)
{
	tree ls_ptr; /* Pointer to the local storage struct. */
	bool need_ls; /* whether the local storage is needed */

	//<>
	fprintf(stderr, "[DBG, calls] Processing function \"%s\".\n",
		current_function_name());
	//<>

	ls_ptr = create_tmp_var(ptr_type_node);
	mark_addressable(ls_ptr);
	TREE_THIS_VOLATILE(ls_ptr) = 1;

	need_ls = instrument_function(ls_ptr);
	/*
	 * Instrument entry and the exit of the function only if we have
	 * instrumented something in it.
	 * 
	 * [NB] In the future, it might also be needed to instrument
	 * entries and exits of the functions if they are the callbacks
	 * we are interested in.
	 */
	if (need_ls) {
		instrument_fentry(ls_ptr);
		instrument_fexit(ls_ptr);
	}
	return 0;
}
/* ====================================================================== */

/* This pass should run after GIMPLE optimization passes. */
static const struct pass_data kedr_i13n_pass_data = {
		.type = 	GIMPLE_PASS,
		.name = 	"kedr-i13n",
		.optinfo_flags = OPTGROUP_NONE,
		.tv_id = 	TV_NONE,
		.properties_required = PROP_ssa | PROP_cfg,
		.properties_provided = 0,
		.properties_destroyed = 0,
		.todo_flags_start = 0,
		.todo_flags_finish = TODO_update_ssa
};

namespace {
	class kedr_i13n_pass : public gimple_opt_pass {
	public:
		kedr_i13n_pass()
			: gimple_opt_pass(kedr_i13n_pass_data, g)
		{}

		/* opt_pass methods: */
		opt_pass *clone () { return new kedr_i13n_pass(); }
		virtual bool gate (function *) { return true; }
		virtual unsigned int execute (function *f)
		{
			return execute_pass(f);
		}
	}; /* class kedr_i13n_pass */
}  /* anon namespace */
/* ====================================================================== */

int
plugin_init(struct plugin_name_args *plugin_info,
	    struct plugin_gcc_version *version)
{
	struct register_pass_info pass_info;

	if (!plugin_default_version_check(version, &gcc_version))
		return 1;

	// TODO: help string for the plugin, etc.

	pass_info.pass = new kedr_i13n_pass();
	/* "tsan0" runs after all optimizations (if any are used) */
	pass_info.reference_pass_name = "tsan0";
	pass_info.ref_pass_instance_number = 1;
	pass_info.pos_op = PASS_POS_INSERT_BEFORE;

	/* Register "kedr-i13n" pass. */
	register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
			  NULL, &pass_info);
	return 0;
}
/* ====================================================================== */
