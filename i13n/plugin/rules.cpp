#include <yaml.h>
#include <cstdlib>
#include <cerrno>

#include <string>
#include <map>
#include <stdexcept>
#include <sstream>
#include <set>

#include "internal.h"
#include "rules.h"

// for debugging and demonstration of the API
#include <iostream>

/*
 * Maximum allowed number of arguments for the target functions.
 */
#define KEDR_MAX_ARGS	16

using namespace std;

/*
 * Mapping:
 * 	"function_name" => rules
 * 	"struct_name@callback_field_name" => rules
 */
typedef map<string, kedr_i13n_ruleset> kedr_rule_map;
static kedr_rule_map rules;

static string str_strip(const string &str)
{
	static const string ws =  " \n\r\t";

	size_t beg = str.find_first_not_of(ws);
	if (beg == string::npos)
		return string();

	size_t end = str.find_last_not_of(ws);
	return str.substr(beg, end - beg + 1);
}

/*
 * Check if the string 'argname' has format "argN", where N is a positive
 * integer, not greater than KEDR_MAX_ARGS.
 * If it does, the function returns true and stores N in 'argno',
 * otherwise returns false and leaves 'argno' unchanged.
 */
static bool get_argno(const string &argname, unsigned int &argno)
{
	static const string arg_pfx = "arg";
	if (argname.substr(0, arg_pfx.length()) != arg_pfx)
		return false;

	string remainder = argname.substr(arg_pfx.length());
	char *endp;

	errno = 0;
	unsigned int num = strtoul(remainder.c_str(), &endp, 10);

	if (*endp != '\0' || num == 0 || num > KEDR_MAX_ARGS || errno)
		return false;

	argno = num;
	return true;
}

/* Name of the .yml file being processed - mostly, for error reporting. */
static string yml_file;

class kedr_yaml_event
{
public:
	yaml_event_t event;
	bool valid;

	~kedr_yaml_event()
	{
		if (valid)
			yaml_event_delete(&event);
	}
};

static string format_parse_error(const string &what,
				 unsigned int line, unsigned int col = 0)
{
	ostringstream err_text;

	err_text << yml_file << ":" << line;
	if (col)
		err_text  << ":" << col;
	err_text << ": error: " << what;
	return err_text.str();
}

static string format_yaml_event_error(const string &what,
				      const kedr_yaml_event &yevent)
{
	return format_parse_error(
		what,
		(unsigned int)yevent.event.start_mark.line + 1,
		(unsigned int)yevent.event.start_mark.column + 1);
}

static string format_rule_error(const string &func_name, const string &what,
				const kedr_yaml_event &yevent)
{
	ostringstream err_text;

	err_text << "\"" << func_name << "\": " << what;
	return format_yaml_event_error(err_text.str(), yevent);
}

static void get_next_event(yaml_parser_t &parser, kedr_yaml_event &yevent)
{
	if (!yaml_parser_parse(&parser, &yevent.event)) {
		throw runtime_error(format_parse_error(
			parser.problem,
			parser.problem_mark.line + 1,
			parser.problem_mark.column + 1));
	}

	yevent.valid = true;
}

/* We only need one such object. */
kedr_stmt_parser stmt_parser;

void kedr_stmt_parser::add_arg_target(const char *str)
{
	string arg_str = str;
	size_t idx = stmt->args.size();

	stmt->args.resize(idx + 1);
	kedr_i13n_arg &arg = stmt->args[idx];

	if (arg_str == "ret") {
		if (rule->type != KEDR_I13N_RULE_POST &&
		    rule->type != KEDR_I13N_RULE_EXIT) {
			throw kedr_stmt_parse_error(
				"\"ret\" may only be used in \"post\" and \"exit\" rules");
		}
		arg.type = KEDR_I13N_ARG_RET;
		return;
	}

	unsigned int argno;
	if (get_argno(arg_str, argno)) {
		arg.type = KEDR_I13N_ARG_TARGET;
		arg.argno = argno;
		return;
	}

	/* Might be a temporary local variable, check if it is known. */
	set<string>::iterator it = rule->locals.find(arg_str);
	if (it == rule->locals.end()) {
		throw kedr_stmt_parse_error(
			string("local variable '") + string(arg_str) +
			string("' is not initialized"));
	}

	arg.type = KEDR_I13N_ARG_LOCAL;
	arg.local = &(*it);
}

void kedr_stmt_parser::add_arg_imm(const char *str)
{
	string arg_str = str;
	size_t idx = stmt->args.size();

	stmt->args.resize(idx + 1);
	kedr_i13n_arg &arg = stmt->args[idx];

	char *endp;
	errno = 0;
	long long num = strtoll(arg_str.c_str(), &endp, 0);

	if (*endp != '\0' || errno) {
		throw kedr_stmt_parse_error(
			string("incorrect numeric value: ") + arg_str);
	}

	arg.type = KEDR_I13N_ARG_IMM;
	arg.imm = num;
}

void kedr_stmt_parser::left_paren(const char *str)
{
	if (state == KEDR_PSTATE_FIRST_ID) {
		stmt->func = first_id;
	}
	else if (state != KEDR_PSTATE_LPAREN_EXPECTED) {
		syntax_error(str);
	}

	state = KEDR_PSTATE_ARG_BEGIN;
}

void kedr_stmt_parser::right_paren(const char *str)
{
	if (state != KEDR_PSTATE_ARG_BEGIN && state != KEDR_PSTATE_ARG_END)
		syntax_error(str);

	state = KEDR_PSTATE_FINAL;
}

void kedr_stmt_parser::comma(const char *str)
{
	if (state != KEDR_PSTATE_ARG_END)
		syntax_error(str);

	state = KEDR_PSTATE_ARG_EXPECTED;
}

void kedr_stmt_parser::assign(const char *str)
{
	unsigned int argno;

	if (state != KEDR_PSTATE_FIRST_ID)
		syntax_error(str);

	/* first_id is the name of the local variable assigned to */
	if (first_id.empty() || first_id == "ret" || get_argno(first_id, argno)) {
		throw kedr_stmt_parse_error(
			string("expected name of a local variable before '=', got '") +
			first_id + string("'"));
	}

	/*
	 * Add the identifier from the lhs to the set of locals.
	 * It is OK if this local variable already exists there - we will
	 * reuse it then.
	 */
	pair<set<string>::iterator, bool> p = rule->locals.insert(first_id);
	stmt->lhs = &(*(p.first));
	state = KEDR_PSTATE_FUNC_ID;
}

void kedr_stmt_parser::id(const char *str)
{
	if (state == KEDR_PSTATE_INITIAL) {
		first_id = str;
		state = KEDR_PSTATE_FIRST_ID;
	}
	else if (state == KEDR_PSTATE_FUNC_ID) {
		stmt->func = str;
		state = KEDR_PSTATE_LPAREN_EXPECTED;
	}
	else if (state == KEDR_PSTATE_ARG_BEGIN ||
		 state == KEDR_PSTATE_ARG_EXPECTED) {
		add_arg_target(str);
		state = KEDR_PSTATE_ARG_END;
	}
	else {
		syntax_error(str);
	}
}

void kedr_stmt_parser::number(const char *str)
{
	if (state != KEDR_PSTATE_ARG_BEGIN && state != KEDR_PSTATE_ARG_EXPECTED)
		syntax_error(str);

	add_arg_imm(str);
	state = KEDR_PSTATE_ARG_END;
}

void kedr_stmt_parser::unexpected(const char *str)
{
	throw kedr_stmt_parse_error(string("unexpected: ") + string(str));
}

void kedr_stmt_parser::syntax_error(const char *got)
{
	const char *expected;
	string sgot = string(got);

	switch (state) {
	case KEDR_PSTATE_INITIAL:
		expected = "an identifier";
		break;
	case KEDR_PSTATE_FIRST_ID:
		expected = "'=' or '('";
		break;
	case KEDR_PSTATE_FUNC_ID:
		expected = "an identifier";
		break;
	case KEDR_PSTATE_LPAREN_EXPECTED:
		expected = "'('";
		break;
	case KEDR_PSTATE_ARG_BEGIN:
		expected = "an identifier, a number or ')'";
		break;
	case KEDR_PSTATE_ARG_EXPECTED:
		expected = "an identifier or a number";
		break;
	case KEDR_PSTATE_ARG_END:
		expected = "',' or ')'";
		break;
	case KEDR_PSTATE_FINAL:
		throw kedr_stmt_parse_error(
			string("unexpected text after the statement: ") + sgot);
	default:
		throw kedr_stmt_parse_error(string("internal error"));
	}

	throw kedr_stmt_parse_error(string("expected ") + string(expected) +
				    string(", got '") + sgot + string("'"));
}

void kedr_stmt_parser::parse(const char *text, kedr_i13n_rule &rule,
			     unsigned int lineno)
{
	int ret;

	this->rule = &rule;
	this->stmt = &rule.stmts.back();

	stmt->lineno = lineno;

	state = KEDR_PSTATE_INITIAL;
	first_id = "";

	ret = kedr_do_parse_stmt(text);
	if (ret)
		throw kedr_stmt_parse_error(string("internal error"));

	if (state != KEDR_PSTATE_FINAL)
		throw kedr_stmt_parse_error(string(
			"incomplete statement, perhaps, ')' is missing?"));
}

static void parse_code(const string &code, kedr_i13n_rule &rule /* out */,
		       const kedr_yaml_event &yevent)
{
	unsigned int idx = 0;
	size_t beg = 0;
	size_t end = 0;
	bool has_statements = false;

	rule.lineno = (unsigned int)yevent.event.start_mark.line + 1;

	while (end != string::npos) {
		beg = code.find_first_not_of("\n", end);
		if (beg == string::npos)
			break;
		end = code.find_first_of("\n", beg);

		string line = str_strip(code.substr(beg, end - beg));

		/* Ignore empty lines and #-comments */
		if (line.empty() || line[0] == '#') {
			++idx;
			continue;
		}

		try {
			rule.stmts.resize(rule.stmts.size() + 1);
			stmt_parser.parse(line.c_str(), rule,
					  rule.lineno + idx + 1);
			has_statements = true;
		}
		catch (kedr_stmt_parse_error &err) {
			throw runtime_error(format_parse_error(
				err.msg, rule.lineno + idx + 1));
		}
		++idx;
	}

	if (!has_statements) {
		throw runtime_error(format_parse_error(
			"found no statements in the rule", rule.lineno));
	}

	/* The rule has been parsed and is valid. */
	rule.valid = true;
}

static void parse_rules_for_function(yaml_parser_t &parser,
				     const string &name,
				     kedr_i13n_ruleset &ruleset)
{
	kedr_yaml_event yevent;

	get_next_event(parser, yevent);
	if (yevent.event.type != YAML_MAPPING_START_EVENT) {
		throw runtime_error(format_rule_error(
			name,
			"expected start of a rule for this function",
			yevent));
	}

	while (true) {
		get_next_event(parser, yevent);

		if (yevent.event.type == YAML_MAPPING_END_EVENT)
			break;

		/*
		 * A pair of scalar events (type of handler, list of
		 * statements) is expected.
		 */
		if (yevent.event.type != YAML_SCALAR_EVENT) {
			throw runtime_error(format_rule_error(
				name,
				"expected the type of the handler (pre/post/...)",
				yevent));
		}

		string handler_type = str_strip(
			(const char *)(yevent.event.data.scalar.value));
		kedr_i13n_rule *rule;

		if (handler_type == "pre") {
			rule = &ruleset.pre;
			rule->type = KEDR_I13N_RULE_PRE;
		}
		else if (handler_type == "post") {
			rule = &ruleset.post;
			rule->type = KEDR_I13N_RULE_POST;
		}
		else if (handler_type == "entry") {
			rule = &ruleset.entry;
			rule->type = KEDR_I13N_RULE_ENTRY;
		}
		else if (handler_type == "exit") {
			rule = &ruleset.exit;
			rule->type = KEDR_I13N_RULE_EXIT;
		}
		else {
			throw runtime_error(format_rule_error(
				name,
				"unknown handler type \"" + handler_type + "\"",
				yevent));
		}

		if (rule->valid) {
			throw runtime_error(format_rule_error(
				name,
				"found two or more rules for the \"" + handler_type + "\" handler",
				yevent));
		}

		/* get the "code" of the handler */
		get_next_event(parser, yevent);
		if (yevent.event.type != YAML_SCALAR_EVENT) {
			throw runtime_error(format_rule_error(
				name,
				"expected the list of statements",
				yevent));
		}
		string code = str_strip(
			(const char *)(yevent.event.data.scalar.value));
		parse_code(code, *rule, yevent);
	}
}

static void populate_rule_map(yaml_parser_t &parser)
{
	while (true) {
		kedr_yaml_event yevent;
		get_next_event(parser, yevent);

		if (yevent.event.type == YAML_MAPPING_END_EVENT)
			break;

		if (yevent.event.type == YAML_SCALAR_EVENT) {
			string name = str_strip(
				(const char *)yevent.event.data.scalar.value);

			if (!name.size())
				throw runtime_error(format_yaml_event_error(
					"function name is empty",
					yevent));

			pair<kedr_rule_map::iterator, bool> retp;

			retp = rules.insert(make_pair(name, kedr_i13n_ruleset()));
			bool exists = !retp.second;
			if (exists)
				throw runtime_error(format_rule_error(
					name,
					"found two or more sets of rules for this function",
					yevent));

			kedr_i13n_ruleset &ruleset = retp.first->second;
			parse_rules_for_function(parser, name, ruleset);
		}
		else {
			throw runtime_error(format_yaml_event_error(
				"found no rules for the function",
				yevent));
		}
	}
}

void kedr_parse_rules(FILE *in, const char *fname)
{
	yaml_parser_t parser;
	bool done = false;

	yml_file = fname;

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, in);

	try {
		while (!done) {
			kedr_yaml_event yevent;
			get_next_event(parser, yevent);

			switch (yevent.event.type) {
			case YAML_STREAM_START_EVENT:
			case YAML_DOCUMENT_START_EVENT:
			case YAML_DOCUMENT_END_EVENT:
				break;
			case YAML_STREAM_END_EVENT:
				done = true;
				break;
			case YAML_MAPPING_START_EVENT:
				populate_rule_map(parser);
				break;
			default:
				throw runtime_error(format_yaml_event_error(
					"expected the start of mapping {function => rules}",
					yevent));
			}
		}
	}
	catch (runtime_error &e) {
		cerr << e.what() << endl;
		yaml_parser_delete(&parser);
		exit(1);
	}

	yaml_parser_delete(&parser);
	return;
}

/*
 * Return the rule set for the given function, if present, NULL otherwise.
 */
const kedr_i13n_ruleset *kedr_get_ruleset(const std::string &func)
{
	kedr_rule_map::const_iterator it = rules.find(func);
	if (it == rules.end())
		return NULL;
	return &it->second;
}
