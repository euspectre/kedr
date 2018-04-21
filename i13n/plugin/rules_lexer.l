 /*
  * Lexer for the statements in the rules.
  */
%{
#include <errno.h>

#include "internal.h"
%}

%option yylineno
%option noyywrap

ID	[A-Za-z_][A-Za-z_0-9]*
INT	(-?[1-9][0-9]*)|0|(0x[0-9a-fA-F]+)
ASSIGN	=

%%

 /* Delimiters. */
[(]		{ stmt_parser.left_paren(yytext); }
[)]		{ stmt_parser.right_paren(yytext); }
[,]		{ stmt_parser.comma(yytext); }

 /* Identifiers, integers and operators. */
{INT}		{ stmt_parser.number(yytext); }
{ID}		{ stmt_parser.id(yytext); }
{ASSIGN}	{ stmt_parser.assign(yytext); }

 /* Ignore comments and whitespace. */
#[^\n]*		{ }
[ \t\r\n]	{ }

 /*
  * Ignore semicolons. One might occasionally forget and place them in the
  * rules after statements.
  */
[;]		{ }

 /* A catch-all rule: report syntax error. */
.		{ stmt_parser.unexpected(yytext); return 1; }

<<EOF>>		{ return 0; }


%%

int kedr_do_parse_stmt(const char *text)
{
	int ret;
	YY_BUFFER_STATE buffer_state;

	buffer_state = yy_scan_string(text);
	if (!buffer_state) {
		fprintf(stderr, "Out of memory.\n");
		return -ENOMEM;
	}

	ret = yylex();
	yy_delete_buffer(buffer_state);

	if (ret)
		return -EINVAL;

	return 0;
}
