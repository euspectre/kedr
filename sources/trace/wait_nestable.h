#ifndef WAIT_NESTABLE_H
#define WAIT_NESTABLE_H

#include <linux/version.h>
#include <linux/wait.h>

/* for signal_pending() */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#endif

/* Workaround for mainline commit
 * ac6424b981bc ("sched/wait: Rename wait_queue_t => wait_queue_entry_t") */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
# define wait_queue_t wait_queue_entry_t
#endif

struct wait_queue_nestable
{
    wait_queue_t wait;
    bool* woken_flag_p;
};

/*
 * 'wait' is expected to be 'struct wait_queue_nestable'.
 * 
 * Set wait->woken_flag, then call autoremove_wake_function().
 */
int wake_flag_function(wait_queue_t* wait, unsigned mode, int sync, void* key);

/* Workaround for mainline commit
 * 2055da97389a ("sched/wait: Disambiguate wq_entry->task_list and
 * wq_head->task_list naming") */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
# define DEFINE_WAIT_NESTED(name, woken_flag) struct wait_queue_nestable name = \
{ \
    .wait = {.private = current, .func = wake_flag_function, .entry = LIST_HEAD_INIT((name).wait.entry)}, \
    .woken_flag_p = &(woken_flag) \
}

static inline int wqn_task_list_empty(struct wait_queue_nestable* wqn)
{
	return list_empty(&wqn->wait.entry);
}

#else

# define DEFINE_WAIT_NESTED(name, woken_flag) struct wait_queue_nestable name = \
{ \
    .wait = {.private = current, .func = wake_flag_function, .task_list = LIST_HEAD_INIT((name).wait.task_list)}, \
    .woken_flag_p = &(woken_flag) \
}

static inline int wqn_task_list_empty(struct wait_queue_nestable* wqn)
{
	return list_empty(&wqn->wait.task_list);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0) */

/* 
 * Add nestable wait into given queue if it is not already added.
 * 
 * linux/wait.h does not provide function with corresponded semantic.
 */
void add_wait_queue_nestable(wait_queue_head_t* wq, struct wait_queue_nestable* wqn);
static inline void remove_wait_queue_nestable(wait_queue_head_t* wq, struct wait_queue_nestable* wqn)
{
    remove_wait_queue(wq, &wqn->wait);
}

int wait_flagged_interruptible(bool* woken_flag_p);

#endif /* WAIT_NESTABLE_H */