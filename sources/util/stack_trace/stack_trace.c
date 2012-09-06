/* stack_trace.c
 * Stack trace helpers for payload modules in KEDR. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
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
#include <linux/sched.h>

#include <kedr/util/stack_trace.h>

#if defined(CONFIG_FRAME_POINTER) || \
    defined(CONFIG_UNWIND_INFO) || \
    defined(CONFIG_STACK_UNWIND)
/* The system supports reliable stack traces */

#if defined(CONFIG_STACKTRACE) /* save_stack_trace() is available, use it */

#include <linux/stacktrace.h>
typedef struct stack_trace stack_trace_impl_t;
#define save_stack_trace_impl save_stack_trace

#else /* save_stack_trace() is not available */
/* The following several definitions were taken from the implementation 
 * of save_stack_trace() with only minor changes. 
 * Credits for the original version: 
 * Copyright (C) 2006-2009 Red Hat, Inc., Ingo Molnar <mingo@redhat.com> */

#include <asm/stacktrace.h>
struct stack_trace_impl {
	unsigned int nr_entries;
	unsigned int max_entries;
	unsigned long *entries;
	int skip; 
};

/*static void 
save_stack_warning_impl(void *data, char *msg)
{
}

static void
save_stack_warning_symbol_impl(void *data, char *msg, unsigned long symbol)
{
}*/

static int 
save_stack_stack_impl(void *data, char *name)
{
	return 0;
}

static void 
save_stack_address_impl(void *data, unsigned long addr, int reliable)
{
	struct stack_trace_impl *trace = data;
	if (!reliable)
		return;
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = addr;
}

static const struct stacktrace_ops save_stack_ops = {
	/*.warning	= save_stack_warning_impl,*/
	/*.warning_symbol	= save_stack_warning_symbol_impl,*/
	.stack		= save_stack_stack_impl,
	.address	= save_stack_address_impl,
	.walk_stack	= print_context_stack,
};

static void save_stack_trace_impl(struct stack_trace_impl *trace)
{
	dump_trace(current, NULL, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}

typedef struct stack_trace_impl stack_trace_impl_t;
#endif /* defined(CONFIG_STACKTRACE) */

void
kedr_save_stack_trace(unsigned long *entries, unsigned int max_entries,
	unsigned int *nr_entries,
	unsigned long first_entry)
{
	unsigned int i = 0;
	int found = 0;
	unsigned long stack_entries[KEDR_NUM_FRAMES_INTERNAL];
	stack_trace_impl_t trace = {
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
	
	save_stack_trace_impl(&trace);
	
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

#else 
/* The system does not support reliable stack traces. 
 * In this case, we provide a reduced version of kedr_save_stack_trace()
 * that "obtains" only one stack frame, namely, the one passed to it as 
 * 'first_entry'. */
void
kedr_save_stack_trace(unsigned long *entries, unsigned int max_entries,
	unsigned int *nr_entries,
	unsigned long first_entry)
{
	BUG_ON(entries == NULL);
	BUG_ON(nr_entries == NULL);
	BUG_ON(max_entries > KEDR_MAX_FRAMES);
	
	if (max_entries == 0) {
		*nr_entries = 0;
		return;
	}
	
	/* The only entry to be stored. */
	*nr_entries = 1;
	entries[0] = first_entry;
}
#endif

/* ====================================================================== */
