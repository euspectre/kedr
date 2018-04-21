#ifndef _KEDR_RULE_PARSER_H
#define _KEDR_RULE_PARSER_H

#include <vector>
#include <string>
#include <set>

enum kedr_i13n_arg_type
{
	KEDR_I13N_ARG_LOCAL,  /* a temporary local variable */
	KEDR_I13N_ARG_TARGET, /* an argument of the target function */
	KEDR_I13N_ARG_RET,    /* return value of the target function */
	KEDR_I13N_ARG_IMM,    /* an immediate integer value */
};

struct kedr_i13n_arg
{
	enum kedr_i13n_arg_type type;

	union {
		const std::string *local; /* type == KEDR_I13N_ARG_LOCAL */
		unsigned int argno; /* type == KEDR_I13N_ARG_TARGET */
		long long imm; /* type == KEDR_I13N_ARG_IMM */
	};
	/* If type == KEDR_I13N_ARG_RET, these values are ignored. */
};

/*
 * A single statement.
 */
struct kedr_i13n_statement
{
	/*
	 * Pointer to the name of the local variable assigned to in this
	 * statement. NULL if the statement has no lhs.
	 */
	const std::string *lhs;

	/* Name of the function/operation called here. */
	std::string func;

	/* Arguments of the function/operation. */
	std::vector<kedr_i13n_arg> args;

	/* Line number for this statement in the rules file. */
	unsigned int lineno;
};

enum kedr_i13n_rule_type
{
	KEDR_I13N_RULE_PRE,   /* applied before the function call */
	KEDR_I13N_RULE_POST,  /* applied after the function call */
	KEDR_I13N_RULE_ENTRY, /* applied on entry to the function */
	KEDR_I13N_RULE_EXIT,  /* applied right before the function returns */
};

/*
 * A rule for instrumentation of some code construct.
 */
struct kedr_i13n_rule
{
	/* If false, this rule is empty or invalid and cannot be used. */
	bool valid;

	enum kedr_i13n_rule_type type;

	/* The local variables to be added. */
	std::set<std::string> locals;

	/* The list of statements in this rule. */
	std::vector<kedr_i13n_statement> stmts;

	/* Line number for this rule in the rules file. */
	unsigned int lineno;
};

/*
 * The rules for the function calls (pre, post) and for the entry/exit
 * handling of callbacks.
 */
struct kedr_i13n_ruleset
{
	kedr_i13n_rule pre;
	kedr_i13n_rule post;
	kedr_i13n_rule entry;
	kedr_i13n_rule exit;
};

void kedr_parse_rules(FILE *in, const char *fname);
const kedr_i13n_ruleset *kedr_get_ruleset(const std::string &func);

#endif /* _KEDR_RULE_PARSER_H */
