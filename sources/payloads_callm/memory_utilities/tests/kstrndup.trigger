[group]
function.name = kstrndup
trigger.code =>>
    const char *orig_str = "Some string";
    char *result_str = NULL;
    result_str = kstrndup(orig_str, 2, GFP_KERNEL);
    kfree(result_str);
<<
