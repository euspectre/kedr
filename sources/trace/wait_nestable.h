#ifndef WAIT_NESTABLE_H
#define WAIT_NESTABLE_H

#include <linux/wait.h>

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

#define DEFINE_WAIT_NESTED(name, woken_flag) struct wait_queue_nestable name = \
{ \
    .wait = {.private = current, .func = wake_flag_function, .task_list = LIST_HEAD_INIT((name).wait.task_list)}, \
    .woken_flag_p = &(woken_flag) \
}

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