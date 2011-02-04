const char *orig_str = "Some string";
char *result_str = NULL;
result_str = kstrdup(orig_str, GFP_KERNEL);
kfree(result_str);