/* ========================================================================
 * Copyright (C) 2016-2017, Evgenii Shatokhin <eugene.shatokhin@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ftrace.h>
#include <linux/list.h>
#include <linux/kallsyms.h>
#include <linux/rcupdate.h>
#include <linux/preempt.h>

#include <linux/kedr.h>

#define KEDR_PREFIX "kedr: "
/* ====================================================================== */

//<>
#ifndef CONFIG_KEDR_LEAK_CHECKER
# error CONFIG_KEDR_LEAK_CHECKER is not defined
#endif
//<>

MODULE_AUTHOR("Evgenii Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

struct kedr_object {
	struct list_head list;
	struct module *mod;	/* NULL for vmlinux, non-NULL for modules */
	struct list_head funcs;
};

struct kedr_func {
	struct list_head list;
	struct ftrace_ops ops;

	/* This handler will be called instead of the stub. */
	void (*handler)(struct kedr_local *local);

	/* Address of the stub the handler is attached to. */
	unsigned long addr;
};
/* ====================================================================== */

/*
 * This mutex protects the global lists defined here with all the data they
 * refer to, as well as kedr_enabled variable.
 */
static DEFINE_MUTEX(kedr_mutex);

static LIST_HEAD(kedr_objects);

static bool kedr_enabled;
/* ====================================================================== */

/* The handlers. */
static void alloc_pre(struct kedr_local *local)
{
	// TODO
}

static void alloc_post(struct kedr_local *local)
{
	// TODO
}

static void free_pre(struct kedr_local *local)
{
	// TODO
}

static void free_post(struct kedr_local *local)
{
	// TODO
}
/* ====================================================================== */



static int kedr_kallsyms_callback(void *data, const char *name,
				  struct module *mod, unsigned long addr)
{
	static const char stub_prefix[] = "kedr_stub_";
	static size_t prefix_len = ARRAY_SIZE(stub_prefix) - 1;
	struct module *target = data;
	const char *part;

	/*
	 * If 'target' is NULL, we need to check all the symbols.
	 * If 'target' is non-NULL, it specifies the module we are
	 * interested in.
	 */
	if (target && target != mod)
		return 0;

	if (strncmp(name, stub_prefix, prefix_len) != 0)
		return 0;

	part = name + prefix_len;
	if (strcmp(part, "alloc_pre") == 0) {
		// TODO
		pr_info(KEDR_PREFIX "Got alloc_pre() in %s at %p.\n",
			module_name(mod), (void *)addr);
	}
	else if (strcmp(part, "alloc_post") == 0) {
		// TODO
		pr_info(KEDR_PREFIX "Got alloc_post() in %s at %p.\n",
			module_name(mod), (void *)addr);
	}
	else if (strcmp(part, "free_pre") == 0) {
		// TODO
		pr_info(KEDR_PREFIX "Got free_pre() in %s at %p.\n",
			module_name(mod), (void *)addr);
	}
	else if (strcmp(part, "free_post") == 0) {
		// TODO
		pr_info(KEDR_PREFIX "Got free_post() in %s at %p.\n",
			module_name(mod), (void *)addr);
	}
	else {
		pr_info(KEDR_PREFIX "Unknown KEDR stub \"%s\" in %s.\n",
			name, module_name(mod));
		return 0;
	}

	// TODO: attach the handler. Keep the state (successfully attached or not) in the structure?
	// Or just create the structures and attach the handlers later?
	return 0;
}

/*
 * Find KEDR stubs in the code and attach the appropriate handlers to them.
 * If mod is non-NULL, search the given module only, otherwise search
 * everywhere.
 */
static int kedr_attach_handlers(struct module *mod)
{
	int ret;

	ret = mutex_lock_killable(&module_mutex);
	if (ret)
		return ret;
	kallsyms_on_each_symbol(kedr_kallsyms_callback, mod);
	mutex_unlock(&module_mutex);
	return 0;
}

/*
 * Detach the handlers from the KEDR stubs in the given module (if mod is
 * not NULL) or everywhere (if mod is NULL).
 */
static void kedr_detach_handlers(struct module *mod)
{
	// TODO
}
/* ====================================================================== */

static int kedr_module_notify(struct notifier_block *nb, unsigned long action,
			      void *data)
{
	struct module *mod = data;
	int ret;

	/*
	 * We check kedr_enabled here just in case this notification came
	 * right before KEDR was disabled. kedr_mutex is used to serialize
	 * the events w.r.t. enabling/disabling of KEDR.
	 */
	switch(action) {
	case MODULE_STATE_COMING:
		ret = mutex_lock_killable(&kedr_mutex);
		if (ret) {
			pr_warning(KEDR_PREFIX "Failed to lock kedr_mutex.\n");
			break;
		}
		if (kedr_enabled)
			ret = kedr_attach_handlers(mod);
		if (ret)
			pr_warning(
				KEDR_PREFIX
				"Failed to attach handlers to \"%s\", errno: %d.\n",
				module_name(mod), ret);
		mutex_unlock(&kedr_mutex);
		break;
	case MODULE_STATE_GOING:
		mutex_lock(&kedr_mutex);
		if (kedr_enabled)
			kedr_detach_handlers(mod);
		mutex_unlock(&kedr_mutex);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block kedr_module_nb = {
	.notifier_call = kedr_module_notify,
	.priority = -1, /* let others do their work first */
};
/* ====================================================================== */

/* Set up and enable event handling. */
static int kedr_enable(void)
{
	int ret = 0;

	ret = mutex_lock_killable(&kedr_mutex);
	if (ret != 0) {
		pr_warning(KEDR_PREFIX "Failed to lock kedr_mutex.\n");
		return ret;
	}

	ret = register_module_notifier(&kedr_module_nb);
	if (ret) {
		pr_warning(KEDR_PREFIX
			   "Failed to register the module notifier.\n");
		goto out_unlock;
	}

	ret = kedr_attach_handlers(NULL);
	if (ret)
		goto out_unreg;

	kedr_enabled = true;
	mutex_unlock(&kedr_mutex);
	return 0;

out_unreg:
	unregister_module_notifier(&kedr_module_nb);
out_unlock:
	mutex_unlock(&kedr_mutex);
	return ret;
}

/* Disable event handling. */
static void kedr_disable(void)
{
	mutex_lock(&kedr_mutex);
	kedr_enabled = false;
	unregister_module_notifier(&kedr_module_nb);
	kedr_detach_handlers(NULL);
	mutex_unlock(&kedr_mutex);
}
/* ====================================================================== */

static int __init kedr_init(void)
{
	int ret;
	// TODO: prepare everything the handlers may use.

	ret = kedr_enable();
	if (ret)
		return ret;

	// TODO: other initialization not directly related to event handling:
	// debugfs knobs, etc.
	return 0;
}

static void __exit kedr_exit(void)
{
	/* TODO: cleanup the stuff not directly related to event handling:
	   debugfs knobs, etc. */

	kedr_disable();

	/*
	 * kedr_disable() detached the handlers, they will no longer
	 * start until re-attached.
	 *
	 * However, some handlers might have already started before they
	 * were detached, so let us wait for them to finish.
	 *
	 * The handlers disable preemption, so synchronize_sched() should
	 * do the trick here.
	 */
	synchronize_sched();

	/*
	 * ? Is it possible for a handler to be preempted before it has
	 * called preempt_disable() and resume after synchronize_sched()
	 * has already completed? I suppose it is not but I cannot prove it
	 * yet.
	 *
	 * If is it possible though, we need some other means to make sure
	 * the handlers are not running and will not start at this point,
	 * before we cleanup the resources the handlers might use.
	 */

	/* TODO: cleanup the resources the handlers were using. No handler runs at this point. */
	return;
}

module_init(kedr_init);
module_exit(kedr_exit);
/* ====================================================================== */

//<> handler
/*
preempt_disable();
	<do the handler's job>
preempt_enable();
------------
Then you can wait for all currently running handlers to complete by calling
synchronize_sched().

I assume the handlers do not need to check kedr_enabled.
*/
//<>
