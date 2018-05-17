#ifndef _KEDR_RULE_PARSER_INTERNAL_H
#define _KEDR_RULE_PARSER_INTERNAL_H

#include <string>

struct kedr_i13n_rule;
struct kedr_i13n_statement;

class kedr_stmt_parser
{
public:
	/*
	 * Main parsing routine, populates the last statement in the given
	 * rule.
	 * text - the code to parse.
	 */
	void parse(const char *text, kedr_i13n_rule &rule,
		   unsigned int lineno);

	/*
	 * The following methods are called when the appropriate tokens
	 * are found.
	 */
	void left_paren(const char *str);
	void right_paren(const char *str);
	void comma(const char *str);
	void assign(const char *str);
	void id(const char *str);
	void number(const char *str);

	/* Syntax errors */
	void unexpected(const char *str);
	void syntax_error(const char *got);

private:
	kedr_i13n_rule *rule; /* the rule info to fill */
	kedr_i13n_statement *stmt; /* the parsed statement */

	enum kedr_i13n_parser_state
	{
		KEDR_PSTATE_INITIAL,
		KEDR_PSTATE_FIRST_ID,
		KEDR_PSTATE_FUNC_ID,
		KEDR_PSTATE_LPAREN_EXPECTED,
		KEDR_PSTATE_ARG_BEGIN,
		KEDR_PSTATE_ARG_EXPECTED,
		KEDR_PSTATE_ARG_END,
		KEDR_PSTATE_FINAL
	} state;

	std::string first_id;

private:
	void add_arg_target(const char *str);
	void add_arg_imm(const char *str);
};

class kedr_stmt_parse_error
{
public:
	kedr_stmt_parse_error(const std::string &str):
		msg(str)
	{}
public:
	std::string msg;
};

extern kedr_stmt_parser stmt_parser;

int kedr_do_parse_stmt(const char *text);

#endif /* _KEDR_RULE_PARSER_INTERNAL_H */