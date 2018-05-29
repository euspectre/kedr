/* Some of the functions here are based on the implementation of
 * ThreadSanitizer from GCC 5.3 (gcc/tsan.c). */

#include <string>
#include <map>
#include <sstream>

#include <assert.h>
#include <gcc-plugin.h>
#include <plugin-version.h>

#include <cstdlib>	/* getenv */
#include <cstdio>
#include <cstring>
#include <errno.h>

#include <libgen.h>	/* basename() */

#include "common_includes.h"
#include "rules.h"

#define KEDR_PLUGIN_NAME "kedr-i13n"

using namespace std;

/* Use this to mark the symbols to be exported from this plugin. The
 * remaining symbols will not be visible from outside of this plugin even
 * if they are not static (-fvisibility=hidden GCC option is used to achieve
 * this). */
#define PLUGIN_EXPORT __attribute__ ((visibility("default")))

#if BUILDING_GCC_VERSION >= 6000
typedef gimple * kedr_stmt;
#else
typedef gimple kedr_stmt;
#endif

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

/* yml file with the rules. */
static char *rfile;

string error_prefix(unsigned int lineno = 0)
{
	ostringstream pfx;

	pfx << "plugin " << KEDR_PLUGIN_NAME; 
	if (lineno) {
		pfx << ": " << basename(rfile) << ":" << lineno;
	}
	return pfx.str();
}

/* FNDECLs for KEDR helper functions, stubs, etc. */
typedef map<string, tree> kedr_fndecl_map;
static kedr_fndecl_map kedr_fndecls;

static void kedr_add_fndecl(const string &fname, tree fndecl)
{
	kedr_fndecls.insert(make_pair(fname, fndecl));
}

/*
 * Returns FNDECL for a KEDR helper or stub.
 * If not found, this is an error.
 */
static tree kedr_get_fndecl(const string &fname, unsigned int rule_lineno = 0)
{
	kedr_fndecl_map::iterator it;

	it = kedr_fndecls.find(fname);
	if (it == kedr_fndecls.end()) {
		error("%s: unable to find declaration of \"%s\", please check kedr_helpers.h.",
		      error_prefix(rule_lineno).c_str(),
		      fname.c_str());
		return NULL_TREE;
	}
	return it->second;
}

static void
instrument_fentry(tree &ls_ptr)
{
	static tree fentry_decl = NULL_TREE;
	basic_block on_entry;
	gimple_stmt_iterator gsi;
	gcall *g;
	gimple_seq seq = NULL;

	if (fentry_decl == NULL_TREE)
		fentry_decl = kedr_get_fndecl("kedr_fentry");

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

	if (fexit_decl == NULL_TREE)
		fexit_decl = kedr_get_fndecl("kedr_fexit");

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
		kedr_stmt stmt;
		gcall *g;

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
 * mapping: name_of_temporary_var => tree_for_the_var
 */
typedef map<string, tree> kedr_tmp_var_map;

static void kedr_add_temporary(kedr_tmp_var_map &tmp_var_map,
			       const string &fname,
			       const struct kedr_i13n_rule &rule)
{
	if (tmp_var_map.find(fname) != tmp_var_map.end()) {
		/* should not happen, but... */
		error("%s: attempt to create temporary variable \"%s\" twice",
		      error_prefix(rule.lineno).c_str(),
		      fname.c_str());
		return;
	}

	tree decl = create_tmp_var(long_unsigned_type_node);
	tmp_var_map.insert(make_pair(fname, decl));
}

static void kedr_create_temporaries(kedr_tmp_var_map &tmp_vars,
			       const struct kedr_i13n_rule &rule)
{
	set<string>::const_iterator it;

	for (it = rule.locals.begin(); it != rule.locals.end(); ++it)
		kedr_add_temporary(tmp_vars, *it, rule);
}

/*
 * Returns the tree for the given temporary. If not found, this is an error.
 */
static tree kedr_get_temporary(const kedr_tmp_var_map &tmp_vars,
			       const string &tname,
			       const kedr_i13n_statement &st)
{
	kedr_tmp_var_map::const_iterator it;

	it = tmp_vars.find(tname);
	if (it == tmp_vars.end()) {
		/*
		 * Should not happen, but it might if the parser for the
		 * instrumentation rules is broken somehow.
		 */
		error("%s: unable to find temporary variable \"%s\".",
		      error_prefix(st.lineno).c_str(),
		      tname.c_str());
		return NULL_TREE;
	}
	return it->second;
}

/*
 * This temporary local variable will be used as lhs in a statement.
 * As we should maintain SSA, let us use lhs_vars to check if a variable
 * with this name has already been used as lhs for the current rule. If so,
 * create a new variable and update tmp_vars map.
 *
 * This is to handle the statements like this in rules.yml:
 *
 *   size = kedr_helper_foo()
 *   kedr_handle_tat(size)
 *   size = kedr_helper_bar()
 *   kedr_handle_boz(size)
 *
 * After the second assignment, the previous value of 'size' has no effect
 * on the subsequent statements. That is why a new variable is created.
 */
static tree kedr_get_temporary_for_lhs(kedr_tmp_var_map &tmp_vars,
				       set<string> &lhs_vars,
				       const kedr_i13n_statement &st)
{
	kedr_tmp_var_map::iterator it = tmp_vars.find(*st.lhs);
	if (it == tmp_vars.end()) {
		error("%s: unable to find temporary variable \"%s\" for the lhs of a call.",
		      error_prefix(st.lineno).c_str(),
		      st.lhs->c_str());
		return NULL_TREE;
	}

	pair<set<string>::iterator, bool> p = lhs_vars.insert(*st.lhs);
	if (!p.second) {
		/* variable with this name was used as lhs before */
		it->second = create_tmp_var(long_unsigned_type_node);
	}
	return it->second;
}

/*
 * Prepare an argument of the call to be generated. The argument may be one
 * of the target function's arguments, its return value (post-handlers only),
 * a local temporary variable or an integer constant. Type conversions are
 * generated as needed.
 *
 * 'i13n_arg' - specification of the argument to create;
 * 'seq' - GIMPLE sequence to add statements to (for type conversion);
 * 'param_type' - declared type of the relevant parameter of the function
 *   to be called;
 * 'orig_stmt' - the statement which we are instrumenting. i.e. the call
 *   to the target function;
 * 'tmp_vars' - available local temporaries.
 */
static tree prepare_call_arg(const kedr_i13n_arg &i13n_arg,
			     gimple_seq &seq,
			     tree param_type,
			     kedr_stmt orig_stmt,
			     const kedr_tmp_var_map &tmp_vars,
			     const kedr_i13n_statement &st)
{
	tree decl;
	tree orig_fndecl = gimple_call_fndecl(orig_stmt);
	tree ret_type;
	unsigned int orig_argno;

	switch (i13n_arg.type) {
	case KEDR_I13N_ARG_LOCAL:
		decl = kedr_get_temporary(tmp_vars, *i13n_arg.local, st);
		break;

	case KEDR_I13N_ARG_RET:
		ret_type = gimple_call_return_type(as_a<gcall *>(orig_stmt));
		if (types_compatible_p(ret_type, void_type_node)) {
			error("%s: the rule for %s wants its return value but the function returns void.",
			      error_prefix(st.lineno).c_str(),
			      IDENTIFIER_POINTER(DECL_NAME(orig_fndecl)));
			return NULL_TREE;
		}

		decl = gimple_call_lhs(orig_stmt);

		/*
		 * If the return value of the target call is ignored, store
		 * it in a temporary. Our function needs it.
		 */
		if (!decl) {
			decl = create_tmp_var(ret_type);
			gimple_call_set_lhs(orig_stmt, decl);
		}
		break;

	case KEDR_I13N_ARG_TARGET:
		/*
		 * The arguments in the rules are numbered starting from 1
		 * while GCC counts them starting from 0.
		 */
		orig_argno = i13n_arg.argno - 1;
		if (orig_argno >= gimple_call_num_args(orig_stmt)) {
			error("%s: %s has %u argument(s) but the rule wants its argument #%u.",
			      error_prefix(st.lineno).c_str(),
			      IDENTIFIER_POINTER(DECL_NAME(orig_fndecl)),
			      gimple_call_num_args(orig_stmt),
			      i13n_arg.argno);
			return NULL_TREE;
		}

		decl = gimple_call_arg(orig_stmt, orig_argno);
		break;

	case KEDR_I13N_ARG_IMM:
		decl = build_int_cstu(long_unsigned_type_node,
				      (unsigned HOST_WIDE_INT)i13n_arg.imm);
		break;

	default:
		error("%s: unknown type of the argument: %u.",
		      error_prefix(st.lineno).c_str(),
		      (unsigned int)i13n_arg.type);
		return NULL_TREE;
	}

	if (!useless_type_conversion_p(param_type, TREE_TYPE(decl))) {
		tree var = make_ssa_name(param_type);
		kedr_stmt g = gimple_build_assign(var, NOP_EXPR, decl);
		gimple_seq_add_stmt(&seq, g);
		decl = var;
	}
	return decl;
}

/*
 * Process one statement from an instrumentation rule.
 * Returns true if the statement call a handler function, false otherwise.
 */
static bool process_one_statement(const kedr_i13n_statement &st,
				  gimple_seq &seq,
				  kedr_stmt orig_stmt,
				  kedr_tmp_var_map &tmp_vars,
				  set<string> &lhs_vars,
				  tree &ls_ptr)
{
	/*
	 * If kedr_handle_<something>(...) is called in a rule, a call to
	 * kedr_stub_handle_<something>(..., local) should be generated.
	 * Let us change the name accordingly and pass 'local' as the last
	 * argument in that case, otherwise call the function as is.
	 */
	static const string pfx = "kedr_handle_";
	static const string stub_pfx = "kedr_stub_handle_";

	gcall *g;
	tree fndecl;
	vec<tree> args = vNULL;
	size_t n;

	bool is_handler = (st.func.substr(0, pfx.length()) == pfx);
	if (is_handler) {
		fndecl = kedr_get_fndecl(
			stub_pfx + st.func.substr(pfx.length()),
			st.lineno);
	}
	else {
		fndecl = kedr_get_fndecl(st.func, st.lineno);
	}

	if (!fndecl)
		return false; /* should not get here, but... */

	tree parm_type_elem = TYPE_ARG_TYPES(TREE_TYPE(fndecl));
	for (n = 0; n < st.args.size(); ++n) {
		if (!parm_type_elem || parm_type_elem == void_list_node) {
			error("%s: %s is declared with %u argument(s) but the rule uses its argument #%u.",
			      error_prefix(st.lineno).c_str(),
			      IDENTIFIER_POINTER(DECL_NAME(fndecl)),
			      (unsigned int)n,
			      (unsigned int)n + 1);
			return false;
		}
		tree arg = prepare_call_arg(st.args[n], seq,
					    TREE_VALUE(parm_type_elem),
					    orig_stmt, tmp_vars, st);
		args.safe_push(arg);
		parm_type_elem = TREE_CHAIN(parm_type_elem);
	}

	if (is_handler) {
		if (!parm_type_elem || parm_type_elem == void_list_node) {
			/* no space for 'kedr_local *', it seems */
			error("%s: the rule must not set argument #%u of handler %s: it is reserved.",
			      error_prefix(st.lineno).c_str(),
			      (unsigned int)n,
			      IDENTIFIER_POINTER(DECL_NAME(fndecl)));
			return false;
		}

		parm_type_elem = TREE_CHAIN(parm_type_elem);
		args.safe_push(ls_ptr);
	}

	if (parm_type_elem && parm_type_elem != void_list_node) {
		error("%s: too few (%u) arguments for %s.",
		      error_prefix(st.lineno).c_str(),
		      (unsigned int)st.args.size(),
		      IDENTIFIER_POINTER(DECL_NAME(fndecl)));
		return false;
	}

	g = gimple_build_call_vec(fndecl, args);

	if (st.lhs) {
		tree lhs = kedr_get_temporary_for_lhs(tmp_vars, lhs_vars, st);
		if (lhs)
			gimple_call_set_lhs(g, lhs);
	}
	gimple_seq_add_stmt(&seq, g);
	return is_handler;
}

static void
process_one_rule(const kedr_i13n_rule &rule, gimple_seq &seq,
		 kedr_stmt orig_stmt, tree &ls_ptr)
{
	bool handler_called = false;

	seq = NULL;

	/*
	 * May happen if the rules file does not specify the given rule
	 * (no "pre" rules, etc.).
	 * Not an error.
	 */
	if (!rule.valid)
		return;

	kedr_tmp_var_map tmp_vars;
	set<string> lhs_vars;

	kedr_create_temporaries(tmp_vars, rule);
	for (size_t i = 0; i < rule.stmts.size(); ++i) {
		handler_called |= process_one_statement(
			rule.stmts[i], seq, orig_stmt, tmp_vars, lhs_vars,
			ls_ptr);
	}

	if (!handler_called) {
		error("%s: found no handler calls in the rule.",
		      error_prefix(rule.lineno).c_str());
		return;
	}
}

/*
 * Add the handlers for the function calls of interest.
 * 'ls_ptr' - pointer to the local storage (struct kedr_local).
 *
 * Returns true if it has instrumented the call, false otherwise.
 */
static bool
instrument_function_call(gimple_stmt_iterator *gsi, tree &ls_ptr)
{
	static const string pfx = "__real_";

	kedr_stmt stmt = gsi_stmt(*gsi);
	gimple_seq seq_pre = NULL;
	gimple_seq seq_post = NULL;

	tree fndecl = gimple_call_fndecl(stmt);
	if (!fndecl)
		return false; /* Indirect call, nothing to do. */

	const char *name = IDENTIFIER_POINTER(DECL_NAME(fndecl));
	const kedr_i13n_ruleset *rs;

	/*
	 * If CONFIG_FORTIFY_SOURCE is set in the kernel, additional checks
	 * are made in the string functions. For example, kmemdup() becomes
	 * inline, checks for potential overflows and the calls
	 * __real_kmemdup() - it is this call that the plugin may see here.
	 * __real_kmemdup() is then renamed into kmemdup() again, but it
	 * seems to be done at a later stage.
	 *
	 * See the mainline commit 6974f0c4555e "include/linux/string.h:
	 * add the option of fortified string.h functions".
	 *
	 * As a workaround, let us check if the function has "__real_"
	 * prefix and if so, lookup the rule set for the name without that
	 * prefix.
	 */
	if (strncmp(name, pfx.c_str(), pfx.length()) == 0) {
		rs = kedr_get_ruleset(&name[pfx.length()]);
	}
	else {
		rs = kedr_get_ruleset(name);
	}
	if (!rs)
		return false;

	/* pre */
	process_one_rule(rs->pre, seq_pre, stmt, ls_ptr);
	if (seq_pre)
		gsi_insert_seq_before(gsi, seq_pre, GSI_SAME_STMT);

	/* post */
	process_one_rule(rs->post, seq_post, stmt, ls_ptr);
	if (seq_post)
		gsi_insert_seq_after(gsi, seq_post, GSI_CONTINUE_LINKING);

	return (seq_pre || seq_post);
}

static bool
instrument_gimple(gimple_stmt_iterator *gsi, tree &ls_ptr)
{
	kedr_stmt stmt = gsi_stmt(*gsi);
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

static unsigned int
execute_pass(function */*f*/)
{
	tree ls_ptr; /* Pointer to the local storage (struct kedr_local). */
	bool need_ls; /* whether the local storage is needed */

	ls_ptr = create_tmp_var(ptr_type_node);
	mark_addressable(ls_ptr);
	TREE_THIS_VOLATILE(ls_ptr) = 1; // TODO: check if it is needed to mark it volatile.

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

/* This pass should run after GIMPLE optimization passes. */
static const struct pass_data kedr_i13n_pass_data = {
		.type = 	GIMPLE_PASS,
		.name = 	KEDR_PLUGIN_NAME,
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

/*
 * Save the function decl trees we need, for future use.
 */
static void on_finish_decl(void *event_data, void * /* data */)
{
	static const string kedr_pfx = "kedr_";
	tree decl = (tree)event_data;

	if (decl == NULL_TREE || decl == error_mark_node ||
	    TREE_CODE(decl) != FUNCTION_DECL) {
		return;
	}

	string fname = fndecl_name(decl);
	if (fname.substr(0, kedr_pfx.length()) != kedr_pfx)
		return;

	kedr_add_fndecl(fname, decl);
}

int
plugin_init(struct plugin_name_args *plugin_info,
	    struct plugin_gcc_version *version)
{
	struct register_pass_info pass_info;

	if (!plugin_default_version_check(version, &gcc_version)) {
		fprintf(stderr,
			"Error: \"%s\" plugin was built for a different version of GCC.\n",
			KEDR_PLUGIN_NAME);
		return 1;
	}

	rfile = getenv("KEDR_RULES_FILE");
	if (!rfile || rfile[0] == 0) {
		fprintf(stderr,
			"Error: environment variable KEDR_RULES_FILE is not set or is empty.\n");
		return 1;
	}

	string rules_file(rfile);
	FILE *rfl = fopen(rules_file.c_str(), "rb");
	if (!rfl) {
		perror("Error");
		fprintf(stderr, "Error: unable to open %s.\n",
			rules_file.c_str());
		return 1;
	}

	kedr_parse_rules(rfl, rules_file.c_str());
	fclose(rfl);

	pass_info.pass = new kedr_i13n_pass();
	/* "tsan0" runs after all optimizations (if any are used) */
	pass_info.reference_pass_name = "tsan0";
	pass_info.ref_pass_instance_number = 1;
	pass_info.pos_op = PASS_POS_INSERT_BEFORE;

	/* Register the pass. */
	register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
			  NULL, &pass_info);
	/*
	 * The plugin needs to find the declarations of KEDR helpers and
	 * stub functions. Not sure if there is a better way than to check
	 * each declaration as it is processed by the compiler, but it
	 * works.
	 */
	register_callback(plugin_info->base_name, PLUGIN_FINISH_DECL,
			  on_finish_decl, NULL);

	fprintf(stderr, "Using GCC plugin \"%s\".\n", KEDR_PLUGIN_NAME);
	return 0;
}
