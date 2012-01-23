/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Copyright (C) 2010-2011, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/string.h> /* strcmp() */

#include <kedr/core/kedr.h>

char* function_name = NULL;
module_param(function_name, charp, S_IRUGO);

int is_intercepted = 0;
module_param(is_intercepted, int, S_IRUGO);

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

<$if concat(header)$><$header: join(\n)$>
<$endif$>

<$if concat(function.name)$><$block : join(\n)$>
<$endif$>
struct interception
{
	const char* function_name;
	void* target_function;
	void* post_function;
};

static const struct interception
interceptions[] =
{
	<$if concat(function.name)$><$interception: join(\n\t)$>
	<$endif$>{NULL, NULL, NULL}
};

static struct kedr_post_pair post_pairs[] =
{
    /* Should be filled at module init stage */
    {
        .orig = NULL,
        .post = NULL
	},
    {
        .orig = NULL
    }
};

static struct kedr_payload interception_payload = {
    .mod        = THIS_MODULE,

    .post_pairs = post_pairs
};

extern int functions_support_register(void);
extern void functions_support_unregister(void);


static int __init
payload_init(void)
{
	int result;
	const struct interception* interception;
	
	
	
	if(function_name == NULL)
	{
		pr_err("'function_name' parameter should be set for module when it is inserted.");
		return -EINVAL;
	}
	for(interception = interceptions; interception->function_name != NULL; interception++)
	{
		if(strcmp(interception->function_name, function_name) == 0)
			break;
	}
	if(interception->function_name == NULL)
	{
		pr_err("Replacement is not exist for function '%s'.", function_name);
		return -EINVAL;
	}

	post_pairs[0].orig = interception->target_function;
    post_pairs[0].post  = interception->post_function;
    
    result = functions_support_register();
    if(result) return result;
	
	result = kedr_payload_register(&interception_payload);
	if(result)
	{
		pr_err("Cannot register payload module for verify call interception.");
        functions_support_unregister();
        return result;
	}

	return 0;
}

static void
payload_exit(void)
{
	kedr_payload_unregister(&interception_payload);
    functions_support_unregister();
}

module_init(payload_init);
module_exit(payload_exit);
