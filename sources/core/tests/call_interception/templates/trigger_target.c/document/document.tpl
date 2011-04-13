/* ========================================================================
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

char* function_name;
module_param(function_name, charp, S_IRUGO);

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

<$if concat(header)$><$header: join(\n)$>
<$endif$>

<$if concat(function.name)$><$trigger_function : join(\n)$>
<$endif$>

struct trigger
{
	const char* function_name;
	void (*trigger_function)(void);
};

const struct trigger triggers[] =
{
	<$if concat(function.name)$><$trigger: join(\n\t)$>
	<$endif$>{NULL, NULL}
};

int __init
trigger_target_init(void)
{
	const struct trigger* trigger;
	if(function_name == NULL)
	{
		pr_err("'function_name' parameter should be set for module when it is inserted.");
		return -EINVAL;
	}
	for(trigger = triggers; trigger->function_name != NULL; trigger++)
	{
		if(strcmp(trigger->function_name, function_name) == 0)
			break;
	}
	if(trigger->function_name == NULL)
	{
		pr_err("Trigger is not exist for function '%s'.", function_name);
		return -EINVAL;
	}
	trigger->trigger_function();
	return 0;

}
void
trigger_target_exit(void)
{
}

module_init(trigger_target_init);
module_exit(trigger_target_exit);
