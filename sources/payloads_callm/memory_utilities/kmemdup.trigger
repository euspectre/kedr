const char *orig_data = "Some data";
void *result = NULL;
result = kmemdup(&orig_data[0], 2, GFP_KERNEL);
kfree(result);