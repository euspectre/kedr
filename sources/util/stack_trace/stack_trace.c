/* stack_trace.c
 * Stack trace helpers for payload modules in KEDR. */

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

#include <linux/kernel.h>
#include <linux/stacktrace.h>

#include <kedr/util/stack_trace.h>

void
kedr_save_stack_trace(unsigned long *entries, unsigned int max_entries,
	unsigned int *nr_entries,
	unsigned long first_entry)
{
	unsigned int i = 0;
	int found = 0;
	unsigned long stack_entries[KEDR_NUM_FRAMES_INTERNAL];
	struct stack_trace trace = {
		.nr_entries = 0,
		.entries = &stack_entries[0],
		
		/* Request as many entries as we can. */
		.max_entries = KEDR_NUM_FRAMES_INTERNAL,
			
		/* We need all frames, we'll do filtering ourselves. */
		.skip = 0
	}; 
	
	BUG_ON(entries == NULL);
	BUG_ON(nr_entries == NULL);
	BUG_ON(max_entries > KEDR_MAX_FRAMES);
	
	if (max_entries == 0) {
		*nr_entries = 0;
		return;
	}
	
	save_stack_trace(&trace);
	
	/* At least one entry will be stored. */
	*nr_entries = 1;
	entries[0] = first_entry;
	
	for (i = 0; i < trace.nr_entries; ++i)
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
