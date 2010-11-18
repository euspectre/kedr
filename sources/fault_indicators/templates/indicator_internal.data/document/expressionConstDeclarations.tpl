<$if concat(expression.constant.name)$>static struct kedr_calc_const general_constants[] = {
    <$expressionConstGDeclaration : join(,\n    )$>
};
<$if concat(expression.constant.c_name)$>struct kedr_calc_const c_constants[] = {
    <$expressionConstCDeclaration : join(,\n    )$>
};<$endif$>
static struct kedr_calc_const_vec all_constants[] = {
	{ .n_elems = ARRAY_SIZE(general_constants), .elems = general_constants}<$if concat(expression.constant.c_name)$>,
    { .n_elems = ARRAY_SIZE(c_constants), .elems = c_constants}<$endif$>
};<$else$><$if concat(expression.constant.c_name)$>static struct kedr_calc_const c_constants[] = {
    <$expressionConstCDeclaration : join(,\n    )$>
};

static struct kedr_calc_const_vec all_constants[] =
{
	{ .n_elems = ARRAY_SIZE(c_constants), .elems = c_constants}
};<$endif$><$endif$>