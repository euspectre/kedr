/*
 * Based on trace events sample by Steven Rostedt (kernel 2.6.33.1).
 * */

/*
 * If TRACE_SYSTEM is defined, that will be the directory created
 * in the ftrace directory under /sys/kernel/debug/tracing/events/<system>
 *
 * The define_trace.h below will also look for a file name of
 * TRACE_SYSTEM.h where TRACE_SYSTEM is what is defined here.
 * In this case, it would look for sample.h
 *
 * If the header name will be different than the system name
 * (as in this case), then you can override the header name that
 * define_trace.h will look up by defining TRACE_INCLUDE_FILE
 *
 * This file is called trace-events-sample.h but we want the system
 * to be called "sample". Therefore we must define the name of this
 * file:
 *
 * #define TRACE_INCLUDE_FILE trace-events-sample
 *
 * As we do an the bottom of this file.
 *
 * Notice that TRACE_SYSTEM should be defined outside of #if
 * protection, just like TRACE_INCLUDE_FILE.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM cr_watcher

/*
 * Notice that this file is not protected like a normal header.
 * We also must allow for rereading of this file. The
 *
 *  || defined(TRACE_HEADER_MULTI_READ)
 *
 * serves this purpose.
 */
#if !defined(_TRACE_EVENT_CR_WATCHER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_CR_WATCHER_H

/*
 * All trace headers should include tracepoint.h, until we finally
 * make it into a standard header.
 */
#include <linux/tracepoint.h>

/*
 * The TRACE_EVENT macro is broken up into 5 parts.
 *
 * name: name of the trace point. This is also how to enable the tracepoint.
 *   A function called trace_foo_bar() will be created.
 *
 * proto: the prototype of the function trace_foo_bar()
 *   Here it is trace_foo_bar(char *foo, int bar).
 *
 * args:  must match the arguments in the prototype.
 *    Here it is simply "foo, bar".
 *
 * struct:  This defines the way the data will be stored in the ring buffer.
 *    There are currently two types of elements. __field and __array.
 *    a __field is broken up into (type, name). Where type can be any
 *    type but an array.
 *    For an array. there are three fields. (type, name, size). The
 *    type of elements in the array, the name of the field and the size
 *    of the array.
 *
 *    __array( char, foo, 10) is the same as saying   char foo[10].
 *
 * fast_assign: This is a C like function that is used to store the items
 *    into the ring buffer.
 *
 * printk: This is a way to print out the data in pretty print. This is
 *    useful if the system crashes and you are logging via a serial line,
 *    the data can be printed to the console using this "printk" method.
 *
 * Note, that for both the assign and the printk, __entry is the handler
 * to the data structure in the ring buffer, and is defined by the
 * TP_STRUCT__entry.
 */
TRACE_EVENT(called___kmalloc,

	TP_PROTO(size_t size, gfp_t flags, void* result),

	TP_ARGS(size, flags, result),

	TP_STRUCT__entry(
		__field(	size_t,	size		)
		__field(	gfp_t,	flags		)
		__field(	void*,	result		)
	),

	TP_fast_assign(
		__entry->size	= size;
		__entry->flags	= flags;
		__entry->result	= result;
	),

	TP_printk("__kmalloc(%zu, %x), result: %p", 
		__entry->size, 
		(unsigned int)__entry->flags,
		__entry->result)
);

TRACE_EVENT(called_kfree,

	TP_PROTO(void* p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__field(	void*,	p	)
	),

	TP_fast_assign(
		__entry->p	= p;
	),

	TP_printk("kfree(%p)", 
		__entry->p)
);

TRACE_EVENT(called_kmem_cache_alloc,

	TP_PROTO(struct kmem_cache* mc, gfp_t flags, void* result),

	TP_ARGS(mc, flags, result),

	TP_STRUCT__entry(
		__field(	void*,	mc		)
		__field(	gfp_t,	flags		)
		__field(	void*,	result		)
	),

	TP_fast_assign(
		__entry->mc	= mc;
		__entry->flags	= flags;
		__entry->result	= result;
	),

	TP_printk("kmem_cache_alloc(%p, %x), result: %p\n",
		(void*)__entry->mc, 
		(unsigned int)__entry->flags,
		__entry->result)
);

TRACE_EVENT(called_kmem_cache_free,

	TP_PROTO(struct kmem_cache* mc, void* p),

	TP_ARGS(mc, p),

	TP_STRUCT__entry(
		__field(	void*,	mc		)
		__field(	void*,	p		)
	),

	TP_fast_assign(
		__entry->mc	= mc;
		__entry->p	= p;
	),

	TP_printk("kmem_cache_free(%p, %p)\n",
		(void*)__entry->mc,
		(void*)__entry->p)
);

TRACE_EVENT(called_copy_from_user,

	TP_PROTO(void* to, const void __user * from, unsigned long n, long result),

	TP_ARGS(to, from, n, result),

	TP_STRUCT__entry(
		__field(	void*,			to	)
		__field(	const void __user *,	from	)
		__field(	unsigned long,		n	)
		__field(	long,			result	)
	),

	TP_fast_assign(
		__entry->to	= to;
		__entry->from	= from;
		__entry->n	= n;
		__entry->result	= result;
	),

	TP_printk("copy_from_user(%p, %p, %lu), result: %ld\n",
		__entry->to,
		__entry->from,
		__entry->n,
		__entry->result)
);

TRACE_EVENT(called_copy_to_user,

	TP_PROTO(void __user * to, const void* from, unsigned long n, long result),

	TP_ARGS(to, from, n, result),

	TP_STRUCT__entry(
		__field(	void __user *,	to	)
		__field(	const void*,	from	)
		__field(	unsigned long,	n	)
		__field(	long,		result	)
	),

	TP_fast_assign(
		__entry->to	= to;
		__entry->from	= from;
		__entry->n	= n;
		__entry->result	= result;
	),

	TP_printk("copy_to_user(%p, %p, %lu), result: %ld\n",
		__entry->to,
		__entry->from,
		__entry->n,
		__entry->result)
);

#endif

/***** NOTICE! The #if protection ends here. *****/


/*
 * There are several ways I could have done this. If I left out the
 * TRACE_INCLUDE_PATH, then it would default to the kernel source
 * include/trace/events directory.
 *
 * I could specify a path from the define_trace.h file back to this
 * file.
 *
 * #define TRACE_INCLUDE_PATH ../../samples/trace_events
 *
 * But the safest and easiest way to simply make it use the directory
 * that the file is in is to add in the Makefile:
 *
 * CFLAGS_trace-events-sample.o := -I$(src)
 *
 * This will make sure the current path is part of the include
 * structure for our file so that define_trace.h can find it.
 *
 * I could have made only the top level directory the include:
 *
 * CFLAGS_trace-events-sample.o := -I$(PWD)
 *
 * And then let the path to this directory be the TRACE_INCLUDE_PATH:
 *
 * #define TRACE_INCLUDE_PATH samples/trace_events
 *
 * But then if something defines "samples" or "trace_events" as a macro
 * then we could risk that being converted too, and give us an unexpected
 * result.
 */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
/*
 * TRACE_INCLUDE_FILE is not needed if the filename and TRACE_SYSTEM are equal
 */
#define TRACE_INCLUDE_FILE trace-events-cr_watcher
#include <trace/define_trace.h>
