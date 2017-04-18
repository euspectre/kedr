/* ========================================================================
 * Copyright (C) 2016-2017, Evgenii Shatokhin <eugene.shatokhin@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

/*
 * Some parts of this code may be based on the implementation of livepatch
 * in the mainline kernel.
 */

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

enum kedr_func_state {
	KEDR_FUNC_DISABLED,
	KEDR_FUNC_ENABLED
};

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

	/* Information about the function: for messages, etc. */
	char *info;

	enum kedr_func_state state;
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

/*
 * The handlers.
 * Preemption is disabled there, that allows us to use synchronize_sched()
 * later to wait for all running handlers to complete.
 */
static void alloc_pre(struct kedr_local *local)
{
	preempt_disable();
	// TODO
	preempt_enable();
}

static void alloc_post(struct kedr_local *local)
{
	preempt_disable();
	// TODO
	preempt_enable();
}

static void free_pre(struct kedr_local *local)
{
	preempt_disable();
	// TODO
	preempt_enable();
}

static void free_post(struct kedr_local *local)
{
	preempt_disable();
	// TODO
	preempt_enable();
}
/* ====================================================================== */

static void notrace kedr_ftrace_handler(unsigned long ip,
				       unsigned long parent_ip,
				       struct ftrace_ops *fops,
				       struct pt_regs *regs)
{
	struct kedr_func *func;

	func = container_of(fops, struct kedr_func, ops);
	kedr_arch_set_pc(regs, (unsigned long)func->handler);
}

/* ====================================================================== */

/* Note. mod == NULL corresponds to the kernel proper here. */
static struct kedr_object *kedr_find_object(struct module *mod)
{
	struct kedr_object *obj;

	list_for_each_entry(obj, &kedr_objects, list) {
		if (obj->mod == mod)
			return obj;
	}
	return NULL;
}

/* Note. We assume here that the object for 'mod' does not exist yet. */
static struct kedr_object *kedr_create_object(struct module *mod)
{
	struct kedr_object *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (obj) {
		obj->mod = mod;
		INIT_LIST_HEAD(&obj->funcs);
		list_add(&obj->list, &kedr_objects);
	}
	return obj;
}

static void kedr_destroy_func(struct kedr_func *func)
{
	if (!func)
		return;
	kfree(func->info);
	kfree(func);
}

static struct kedr_func *kedr_create_func(
	void (*handler)(struct kedr_local *local),
	unsigned long addr,
	const char *name,
	const char *module_name)
{
	struct kedr_func *func;
	int len;
	static const char *fmt = "%s at %p (%s)";

	func = kzalloc(sizeof(*func), GFP_KERNEL);
	if (!func)
		return NULL;

	len = snprintf(NULL, 0, fmt, name, (void *)addr, module_name) + 1;
	func->info = kzalloc(len, GFP_KERNEL);
	if (!func->info) {
		kfree(func);
		return NULL;
	}
	snprintf(func->info, len, fmt, name, (void *)addr, module_name);

	func->ops.func = kedr_ftrace_handler;
	func->ops.flags = FTRACE_OPS_FL_SAVE_REGS |
			  FTRACE_OPS_FL_DYNAMIC |
			  FTRACE_OPS_FL_IPMODIFY;
	func->handler = handler;
	func->addr = addr;
	func->state = KEDR_FUNC_DISABLED;
	return func;
}

/*
 * Convert a function address into the appropriate ftrace location.
 *
 * Usually this is just the address of the function, but on some architectures
 * it's more complicated so allow them to provide a custom behaviour.
 */
#ifndef kedr_get_ftrace_location
static unsigned long kedr_get_ftrace_location(unsigned long faddr)
{
	return faddr;
}
#endif

static void kedr_func_detach(struct kedr_func *func)
{
	unsigned long ftrace_loc;
	int ret;

	if (func->state != KEDR_FUNC_ENABLED) {
		pr_info(KEDR_PREFIX
			"Handler for the function %s is not enabled.\n",
			func->info);
		return;
	}

	ftrace_loc = kedr_get_ftrace_location(func->addr);
	if (!ftrace_loc) {
		pr_err(KEDR_PREFIX
			"Failed to find ftrace hook for the function %s\n",
			func->info);
		return;
	}

	ret = unregister_ftrace_function(&func->ops);
	if (ret) {
		pr_err(KEDR_PREFIX
			"Failed to unregister ftrace handler for function %s (error: %d)\n",
			func->info, ret);
		return;
	}

	ret = ftrace_set_filter_ip(&func->ops, ftrace_loc, 1, 0);
	if (ret) {
		pr_err(KEDR_PREFIX
			"Failed to remove ftrace filter for function %s (error: %d)\n",
			func->info, ret);
		return;
	}
}

static int kedr_func_attach(struct kedr_func *func)
{
	unsigned long ftrace_loc;
	int ret;

	if (func->state != KEDR_FUNC_DISABLED) {
		pr_err(KEDR_PREFIX
		       "Handler for the function %s is already enabled.\n",
		       func->info);
		return -EINVAL;
	}

	ftrace_loc = kedr_get_ftrace_location(func->addr);
	if (!ftrace_loc) {
		pr_err(KEDR_PREFIX
			"Failed to find ftrace hook for the function %s\n",
			func->info);
		return -EINVAL;
	}

	ret = ftrace_set_filter_ip(&func->ops, ftrace_loc, 0, 0);
	if (ret) {
		pr_err(KEDR_PREFIX
			"Failed to set ftrace filter for function %s (error: %d)\n",
			func->info, ret);
		return -EINVAL;
	}

	ret = register_ftrace_function(&func->ops);
	if (ret) {
		pr_err(KEDR_PREFIX
			"Failed to register ftrace handler for function %s (error: %d)\n",
			func->info, ret);
		ftrace_set_filter_ip(&func->ops, ftrace_loc, 1, 0);
		return -EINVAL;
	}

	func->state = KEDR_FUNC_ENABLED;

	//<>
	pr_warning("[DBG] Attached handler to the function %s.\n", func->info);
	//<>
	return 0;
}

/*
 * Detaches all handlers attached via this object, if any. Frees all the
 * memory allocated for the respective kedr_func instances.
 * Does not free the object itself.
 */
static void kedr_cleanup_object(struct kedr_object *obj)
{
	struct kedr_func *func;
	struct kedr_func *tmp;

	list_for_each_entry_safe(func, tmp, &obj->funcs, list) {
		kedr_func_detach(func);
		list_del(&func->list);
		kedr_destroy_func(func);
	}
}

static void kedr_destroy_all_objects(void)
{
	struct kedr_object *obj;
	struct kedr_object *tmp;

	list_for_each_entry_safe(obj, tmp, &kedr_objects, list) {
		kedr_cleanup_object(obj);
		list_del(&obj->list);
		kfree(obj);
	}
}

static int kedr_kallsyms_callback(void *data, const char *name,
				  struct module *mod, unsigned long addr)
{
	static const char stub_prefix[] = "kedr_stub_";
	static size_t prefix_len = ARRAY_SIZE(stub_prefix) - 1;
	struct module *target = data;
	const char *part;
	struct kedr_object *obj;
	void (*handler)(struct kedr_local *local);
	struct kedr_func *func;

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
		handler = alloc_pre;
	}
	else if (strcmp(part, "alloc_post") == 0) {
		handler = alloc_post;
	}
	else if (strcmp(part, "free_pre") == 0) {
		handler = free_pre;
	}
	else if (strcmp(part, "free_post") == 0) {
		handler = free_post;
	}
	else {
		pr_info(KEDR_PREFIX "Unknown KEDR stub \"%s\" in %s.\n",
			name, module_name(mod));
		return 0;
	}

	obj = kedr_find_object(mod);
	if (!obj) {
		obj = kedr_create_object(mod);
		if (!obj)
			return -ENOMEM;
	}

	func = kedr_create_func(handler, addr, name, module_name(mod));
	if (!func)
		return -ENOMEM;

	list_add(&func->list, &obj->funcs);
	return 0;
}

/*
 * Detach the handlers from the KEDR stubs in the given module (if mod is
 * not NULL) or everywhere (if mod is NULL).
 */
static void kedr_detach_handlers(struct module *mod)
{
	if (mod) {
		struct kedr_object *obj;

		obj = kedr_find_object(mod);
		if (obj) {
			kedr_cleanup_object(obj);
			list_del(&obj->list);
			kfree(obj);
		}
	}
	else {
		kedr_destroy_all_objects();
	}
}

static int kedr_attach_all_for_object(struct kedr_object *obj)
{
	struct kedr_func *func;
	int ret = 0;

	list_for_each_entry(func, &obj->funcs, list) {
		if (func->state != KEDR_FUNC_DISABLED)
			continue;
		ret = kedr_func_attach(func);
		if (ret)
			break;
	}
	return ret;
}

/*
 * Find KEDR stubs in the code and attach the appropriate handlers to them.
 * If mod is non-NULL, search the given module only, otherwise search
 * everywhere.
 */
static int kedr_attach_handlers(struct module *mod)
{
	int ret;
	struct kedr_object *obj;

	ret = mutex_lock_killable(&module_mutex);
	if (ret)
		return ret;

	ret = kallsyms_on_each_symbol(kedr_kallsyms_callback, mod);
	mutex_unlock(&module_mutex);
	if (ret)
		return ret;

	/*
	 * Ftrace code may lock module_mutex too, e.g., when calling
	 * set_all_modules_text_rw(), so we cannot attach the handlers
	 * in the kallsyms callback itself. Do it here instead.
	 */
	if (mod) {
		obj = kedr_find_object(mod);
		if (obj)
			ret = kedr_attach_all_for_object(obj);
	}
	else {
		list_for_each_entry(obj, &kedr_objects, list) {
			ret = kedr_attach_all_for_object(obj);
			if (ret)
				break;
		}
	}

	if (ret)
		kedr_detach_handlers(mod);

	return ret;
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
		if (!kedr_enabled) {
			mutex_unlock(&kedr_mutex);
			break;
		}
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
	int ret;

	ret = mutex_lock_killable(&kedr_mutex);
	if (ret != 0) {
		pr_warning(KEDR_PREFIX "Failed to lock kedr_mutex.\n");
		return ret;
	}
	ret = kedr_attach_handlers(NULL);
	if (ret == 0)
		kedr_enabled = true;

	mutex_unlock(&kedr_mutex);
	return ret;
}

/* Disable event handling. */
static void kedr_disable(void)
{
	mutex_lock(&kedr_mutex);
	kedr_enabled = false;
	kedr_detach_handlers(NULL);
	mutex_unlock(&kedr_mutex);
}
/* ====================================================================== */

static int __init kedr_init(void)
{
	int ret;

	ret = register_module_notifier(&kedr_module_nb);
	if (ret) {
		pr_warning(KEDR_PREFIX
			   "Failed to register the module notifier.\n");
		return ret;
	}

	// TODO: other initialization tasks.

	ret = kedr_enable();
	if (ret)
		goto out_unreg;

	return 0;

out_unreg:
	unregister_module_notifier(&kedr_module_nb);
	return ret;
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

	unregister_module_notifier(&kedr_module_nb);

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
