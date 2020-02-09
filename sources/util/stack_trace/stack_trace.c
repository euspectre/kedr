/* stack_trace.c
 * Stack trace helpers for payload modules in KEDR. */

/* ========================================================================
 * Copyright (C) 2012-2020, KEDR development team
 * Copyright (C) 2010-2012, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/stacktrace.h>

#include <kedr/util/stack_trace.h>

/*
 * See the changes made to stack trace API in mainline commit
 * e9b98e162aa5 "stacktrace: Provide helpers for common stack trace operations".
 * Kernel 5.2+ has stack_trace_save() already.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
noinline unsigned int stack_trace_save(unsigned long *store,
				       unsigned int size,
				       unsigned int skipnr)
{
	struct stack_trace trace = {
		.entries = store,
		.max_entries = size,
		.skip = skipnr + 1,
	};

	save_stack_trace(&trace);
	return trace.nr_entries;
}
#endif

void kedr_save_stack_trace(unsigned long *entries, unsigned int max_entries,
			   unsigned int *nr_entries,
			   unsigned long first_entry)
{
	unsigned int i = 0;
	unsigned int nr;
	int found = 0;
	unsigned long stack_entries[KEDR_NUM_FRAMES_INTERNAL];

	BUG_ON(entries == NULL);
	BUG_ON(nr_entries == NULL);
	BUG_ON(max_entries > KEDR_MAX_FRAMES);
	
	if (max_entries == 0) {
		*nr_entries = 0;
		return;
	}
	
	nr = stack_trace_save(
		&stack_entries[0],
		KEDR_NUM_FRAMES_INTERNAL, /* as many entries as we can get */
		0 /* we need all stack entries here */);
	
	/* At least one entry will be stored. */
	*nr_entries = 1;
	entries[0] = first_entry;
	
	for (i = 0; i < nr; ++i)
	{
		if (*nr_entries >= max_entries) break;

		if (found) {
			entries[*nr_entries] = stack_entries[i];
			++(*nr_entries);
		} else if (stack_entries[i] == first_entry) 
		{
			found = 1;
		}
	}
}
/* ====================================================================== */
