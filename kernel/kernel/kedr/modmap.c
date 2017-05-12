/*
 * ========================================================================
 * Copyright (C) 2017, Evgenii Shatokhin <eugene.shatokhin@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 * ========================================================================
 */

/*
 * KEDR may record events that had happened in the kernel modules unloaded
 * some time after that. Resolving each PC value or address from the call
 * trace for an event right after the event has happened can be costly.
 * Resolving means preparing a string like printk() with "%pS" format does.
 *
 * So, we track module load events and maintain the map of code areas
 * occupied by the modules. When an event is registered, 'current_modmap' is
 * stored there and that allows us to resolve the addresses in that event in
 * the future. The kernel-mode part of KEDR will only provide the addresses
 * in the form like "init+0xoffset [module]" or "core+0xoffset [module]"
 * for the modules (the userspace tools should handle the rest and transform
 * that to the usual "function+0xoffset [module]" form).
 *
 * For the addresses from the kernel proper, "core+0xoffset" form is used.
 * We could just print the addresses using  %pS, but the functions with the
 * same names would become a problem in that case if it was needed to
 * find the corresponding lines of the source code after that.
 * "core+0xoffset" is not ambiguous.
 *
 * We do not consider the addresses from the init code of the kernel.
 *
 * Note. We do not track unloading of the modules here, same for unloading
 * of init areas after the module initialization. The events KEDR registers
 * can only occur in the normal code of the kernel and modules, so no such
 * event from a module's code should occur after the module has been
 * unloaded.
 */

#include <linux/string.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <linux/sort.h>
#include <linux/bsearch.h>
#include <linux/kedr.h>

/*
 * May be less than MODULE_NAME_LEN but should be enough.
 * The value is chosen so, that sizeof(struct kedr_module_area) was 64.
 */
#define KEDR_MODNAME_LEN (64 - sizeof(unsigned long) - sizeof(unsigned int))

/* The data defined in this file must be accessed with kedr_mutex locked. */

struct kedr_module_area
{
	unsigned long	start;	/* start address */
	char 		modname[KEDR_MODNAME_LEN];
	unsigned int	init	: 1,  /* 1 for init area, 0 for core area */
			size	: 31; /* size of the area */
};

struct kedr_module_map
{
	struct kedr_module_map *prev; /* NULL for the base module map */
	unsigned int nr_areas;
	struct kedr_module_area areas[0];
	/* 'areas[]' should be sorted by 'start' for the base module map. */
};

struct kedr_module_map *current_modmap;

/* Limit the number of layers the module map can have. */
#define KEDR_MAX_MODMAP_LAYERS 8192
int avail_modmap_layers;
/* ====================================================================== */

static int compare_addr(const void *key, const void *elt)
{
	unsigned long addr = *(const unsigned long *)key;
	const struct kedr_module_area *area = elt;

	if (addr < area->start)
		return -1;
	else if (addr >= area->start + area->size)
		return 1;
	else
		return 0;
}

static int addr_in_area(unsigned long addr,
			const struct kedr_module_area *area)
{
	return addr >= area->start && addr < area->start + area->size;
}

static struct kedr_module_area *lookup_module_area(
	struct kedr_module_map *modmap, unsigned long addr)
{
	while (modmap->prev) {
		unsigned int i;

		for (i = 0; i < modmap->nr_areas; ++i) {
			if (addr_in_area(addr, &modmap->areas[i]))
				return &modmap->areas[i];
		}
		modmap = modmap->prev;
	}

	/* the base layer, the array of areas is either empty or sorted */
	if (!modmap->nr_areas)
		return NULL;

	return bsearch(&addr, &modmap->areas[0], modmap->nr_areas,
		       sizeof(modmap->areas[0]), compare_addr);
}

static int snprintf_module_addr(char *buf, size_t size, unsigned long addr,
				const struct kedr_module_area *area)
{
	static const char *fmt_mod = "[<%lx>] %s+0x%lx [%s]";
	unsigned long offset = addr - area->start;

	return snprintf(buf, size, fmt_mod, addr,
			(area->init ? "init" : "core"),
			offset, area->modname);
}

/*
 * Determines if the address belongs to the code of the kernel proper or
 * some module and prepare a string representation for it. The resulting
 * string is similar to what printk outputs with "%pS" format but with
 * the offsets w.r.t. the code area ("init" or "core") rather than a
 * function.
 *
 * The caller is responsible for freeing the returned string when it is no
 * longer needed.
 *
 * Returns NULL if memory allocation failed for the string.
 */
char *kedr_resolve_address(unsigned long addr,
			   struct kedr_module_map *modmap)
{
	static const char *fmt_knl = "[<%lx>] core+0x%lx";
	static const char *fmt_raw = "[<%lx>] 0x%lx";
	unsigned long offset;
	char *str;
	int len;

	if (addr >= kedr_stext && addr < kedr_etext) {
		offset = addr - kedr_stext;
		len = snprintf(NULL, 0, fmt_knl, addr, offset) + 1;
		str = kzalloc(len, GFP_KERNEL);
		if (str)
			snprintf(str, len, fmt_knl, addr, offset);
		return str;
	}

	if (modmap) {
		struct kedr_module_area *area;

		area = lookup_module_area(modmap, addr);
		if (area) {
			len = snprintf_module_addr(NULL, 0, addr, area) + 1;
			str = kzalloc(len, GFP_KERNEL);
			if (str)
				snprintf_module_addr(str, len, addr, area);
			return str;
		}
	}

	/* Unable to resolve the address, output it as is. */
	len = snprintf(NULL, 0, fmt_raw, addr, addr) + 1;
	str = kzalloc(len, GFP_KERNEL);
	if (str)
		snprintf(str, len, fmt_raw, addr, addr);
	return str;
}
/* ====================================================================== */

static int has_init(struct module *mod)
{
	return mod->init_layout.base ? 1 : 0;
}

static int has_core(struct module *mod)
{
	return mod->core_layout.base ? 1 : 0;
}

static void fill_module_area(struct kedr_module_area *area,
			     const struct module_layout *layout)
{
	area->start = (unsigned long)layout->base;
	area->size = layout->text_size;
}

struct kedr_modmap_cb
{
	void (*fn)(struct module *, void *);
	void *data;
};

static int modmap_kallsyms_callback(void *data, const char *name,
				    struct module *mod, unsigned long addr)
{
	struct kedr_modmap_cb *cb = data;

	if (!mod || mod->state == MODULE_STATE_UNFORMED)
		return 0;

	if (strcmp(name, "__this_module") != 0)
		return 0;

	cb->fn(mod, cb->data);
	return 0;
}

/*
 * Calls the specified function for each loaded kernel module except the
 * KEDR core itself, passes 'data' there as the second argument.
 * Must be called under module_mutex.
 *
 * Note. As of version 4.11, the kernel does not provide the external
 * modules a legit way to walk the list of the loaded modules. One could
 * try walking it directly, starting from the current module, something like
 * "list_for_each_entry(mod, &THIS_MODULE->list, list) {...}", however, the
 * real list head would be encountered this way too.
 *
 * The safest and easiest workaround I could find is to use kallsyms to
 * look for '__this_module' variables. Each module has one and only one
 * such variable (and its address is available as 'THIS_MODULE' there).
 */
static void for_each_module(void (*fn)(struct module *, void *), void *data)
{
	struct kedr_modmap_cb cb;

	cb.fn = fn;
	cb.data = data;

	kallsyms_on_each_symbol(modmap_kallsyms_callback, &cb);
}

static void count_module_areas(struct module *mod, void *data)
{
	unsigned int *nr_areas = data;

	*nr_areas += has_init(mod) + has_core(mod);
}

static void populate_module_areas(struct module *mod, void *data)
{
	struct kedr_module_map *modmap = data;
	struct kedr_module_area *area;

	if (has_init(mod)) {
		area = &modmap->areas[modmap->nr_areas];
		strlcpy(area->modname, module_name(mod), KEDR_MODNAME_LEN);
		fill_module_area(area, &mod->init_layout);
		area->init = 1;
		++modmap->nr_areas;
	}

	if (has_core(mod)) {
		area = &modmap->areas[modmap->nr_areas];
		strlcpy(area->modname, module_name(mod), KEDR_MODNAME_LEN);
		fill_module_area(area, &mod->core_layout);
		++modmap->nr_areas;
	}
}

static int compare_items(const void *l, const void *r)
{
	const struct kedr_module_area *left = l;
	const struct kedr_module_area *right = r;

	if (left->start < right->start)
		return -1;
	else if (left->start > right->start)
		return 1;
	else
		return 0;
}

static void swap_items(void *l, void *r, int size)
{
	struct kedr_module_area *left = l;
	struct kedr_module_area *right = r;
	struct kedr_module_area tmp;

	memcpy(&tmp, left, sizeof(struct kedr_module_area));
	memcpy(left, right, sizeof(struct kedr_module_area));
	memcpy(right, &tmp, sizeof(struct kedr_module_area));
}

/*
 * It is OK if the below functions fail without propagating the error
 * outside. The module map can become stale as a result and
 * kedr_resolve_address() would resolve some addresses incorrectly but
 * nothing should crash.
 */
void kedr_create_modmap(void)
{
	int ret;
	unsigned int nr_areas = 0;
	struct kedr_module_map *modmap;

	BUG_ON(current_modmap);

	ret = mutex_lock_killable(&module_mutex);
	if (ret) {
		pr_warning(KEDR_PREFIX "Failed to lock module_mutex.\n");
		return;
	}

	for_each_module(count_module_areas, &nr_areas);
	modmap = kzalloc(
		sizeof(*modmap) + nr_areas * sizeof(modmap->areas[0]),
		GFP_KERNEL);
	if (!modmap) {
		pr_debug(KEDR_PREFIX "kedr_create_modmap: out of memory.\n");
		mutex_unlock(&module_mutex);
		return;
	}
	for_each_module(populate_module_areas, modmap);
	mutex_unlock(&module_mutex);

	avail_modmap_layers = KEDR_MAX_MODMAP_LAYERS;

	if (modmap->nr_areas)
		sort(&modmap->areas[0],
		     (size_t)modmap->nr_areas,
		     sizeof(modmap->areas[0]),
		     compare_items,
		     swap_items);

	current_modmap = modmap;
	return;
}

void kedr_free_modmap(void)
{
	while (current_modmap != NULL) {
		struct kedr_module_map *prev = current_modmap->prev;

		kfree(current_modmap);
		current_modmap = prev;
	}
}

static struct kedr_module_map *alloc_modmap_layer(
	struct module *mod, unsigned int nr_areas)
{
	struct kedr_module_map *modmap;
	unsigned int i;

	if (!current_modmap) {
		pr_debug(KEDR_PREFIX "No module map.\n");
		return NULL;
	}

	if (avail_modmap_layers <= 0) {
		pr_warn_once(
			KEDR_PREFIX
			"The limit on module events has been hit. "
			"Module map will not be updated until KEDR is disabled and re-enabled. "
			"The addresses in the collected events may be resolved incorrectly.\n");
		return NULL;
	}

	modmap = kzalloc(sizeof(*modmap) + nr_areas * sizeof(modmap->areas[0]),
			 GFP_KERNEL);
	if (!modmap) {
		pr_debug(KEDR_PREFIX "alloc_modmap_layer: out of memory.\n");
		return NULL;
	}
	--avail_modmap_layers;

	modmap->nr_areas = nr_areas;
	modmap->prev = current_modmap;

	for (i = 0; i < nr_areas; ++i) {
		strlcpy(modmap->areas[i].modname, module_name(mod),
			KEDR_MODNAME_LEN);
	}

	/*
	 * We can set current_modmap right here despite the newly created
	 * layer is not completely filled yet. Some event caught by KEDR
	 * might record the new value of current_modmap.
	 * kedr_resolve_address() cannot race with this code because it
	 * runs with kedr_mutex locked too.
	 * The caught event could not happen in the affected areas of the
	 * module the layer has been created for. So this new layer will
	 * not affect address resolution for that event.
	 */
	current_modmap = modmap;
	return modmap;
}

void kedr_modmap_on_coming(struct module *mod)
{
	unsigned int nr_areas;
	unsigned int i = 0;
	struct kedr_module_map *modmap;
	struct kedr_module_area *area;

	nr_areas = has_init(mod) + has_core(mod);
	if (!nr_areas)
		return;

	modmap = alloc_modmap_layer(mod, nr_areas);
	if (!modmap)
		return;

	if (has_init(mod)) {
		area = &modmap->areas[i];
		++i;
		fill_module_area(area, &mod->init_layout);
		area->init = 1;
	}
	if (has_core(mod)) {
		area = &modmap->areas[i];
		fill_module_area(area, &mod->core_layout);
	}
}
/* ====================================================================== */
