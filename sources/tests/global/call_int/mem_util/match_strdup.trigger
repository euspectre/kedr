[group]
function.name = match_strdup
trigger.code =>>
	char orig_data[] = "Some data";
	substring_t substr;
	char *result = NULL;

	substr.from = &orig_data[1];
	substr.to = &orig_data[4];
	
	result = match_strdup(&substr);
	kfree(result);
<<
