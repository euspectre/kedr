/*
 * The "base" component of KEDR system. 
 * 
 * "kedr-base" keeps a registry of payload modules and provides interface 
 * for them to register / unregister themselves.
 * 
 * When asked, it return all interception information
 * collected from registered payloads.
 */

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
 
/*
 * Based on the base/base.c from KEDR package at 21.03.11.
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/list.h>
#include <linux/hash.h> /* hash_ptr() */

#include <linux/mutex.h>

#include <kedr/core/kedr.h>
#include "kedr_base_internal.h"

#include "config.h"

/* ================================================================ */
/* This string will be used in debug output to specify the name of 
 * the current component of KEDR
 */
#define COMPONENT_STRING "kedr_base: "

/* ================================================================ */
//MODULE_AUTHOR("Eugene A. Shatokhin");
//MODULE_LICENSE("GPL");
/* ================================================================ */


/* 
 * All registered payloads organized into list.
 * This is an element of this list.
 */
struct payload_elem
{
	struct list_head list;
	
	struct kedr_payload *payload;
	
	/* 
	 * Not 0 if functions from this payload are used for target module.
	 * 
	 * Payloads with 'is_used' != 0 cannot be unregistered.
	 */
	int is_used; 
};

/* Look for a given element in the list. */
static struct payload_elem* 
payload_elem_find(struct kedr_payload *payload, struct list_head* payload_list);

static int payload_elem_fix(struct payload_elem* elem);
static void payload_elem_release(struct payload_elem* elem);

static int payload_elem_fix_all(struct list_head *elems);
static void payload_elem_release_all(struct list_head *elems);

/* Call corresponding callbacks for all used payloads in list */
static void payload_elem_load_callback_all(struct list_head *elems, struct module* m);
static void payload_elem_unload_callback_all(struct list_head *elems, struct module* m);


/* Mark all functions which intercepted by payload as used*/
static int
payload_functions_use(struct kedr_payload* payload);
/* Mark all functions which intercepted by payload as unused*/
static void
payload_functions_unuse(struct kedr_payload* payload);


/*
 * Map of functions.
 * 
 * Used for replacement count verification.
 */

struct functions_map_elem
{
	struct hlist_node list;
	
	void* function;
};

struct functions_map
{
	struct hlist_head* heads;
	unsigned int bits;
};

static int
functions_map_init(struct functions_map* map, int n_elems);
static void
functions_map_destroy(struct functions_map* map);

/* Return error if element already exist */
static int
functions_map_add(struct functions_map* map, void* function);
static void
functions_map_remove(struct functions_map* map, void* function);

/* 
 * Return error if payload try to replace function which already replaced.
 */
static int
function_replacements_add_payload(struct functions_map* replacement_map,
	struct kedr_payload* payload);

static void
function_replacements_remove_payload(struct functions_map* replacement_map,
	struct kedr_payload* payload);

/* ================= Global data =================== */

/* operations which are passed when component is initialized. */
static struct kedr_base_operations* kedr_base_ops;

/* List of currently registered payloads */
static struct list_head payload_list;

/*
 * Nonzero if payloads are used now (target module is loaded.).
 */
static int payloads_are_used;

/* 
 * Functions which are replaced by one payload.
 * 
 * Using this table prevents registering payload which replace
 * already replaced function.
 */

static struct functions_map replaced_functions_map;

/* 
 * Pointer to interception information (collected from all payload modules).
 * 
 * Need to store for freeing when target module is unloaded.
 */

static struct kedr_base_interception_info* info_array_current;

/* A mutex to protect access to the global data of kedr-base */
static DEFINE_MUTEX(base_mutex);
/* ================================================================ */

/*
 * Intermediate hash table which is used for different counters
 * for some payload set:
 * 
 * -count of distinct functions, which are used by these payloads,
 * -for every such function, count pre- and post- functions,
 *   registered by payloads, and whether replace function is registered.
 * 
 * Also, this table supports iterator over its elements.
 */

/* Element of the table */
struct function_counters_elem
{
	/* hash-table organization */
	struct hlist_node list;
	/* Public counters */
	int n_pre;
	int is_replaced;
	int n_post;
	
	/* address of the original function - key in the table */
	void* function;
};


struct function_counters_table
{
	/* Only one public field: number of the elements in the table.*/
	int n_functions;
	
	/* hash table organization */
	struct hlist_head* heads;
	unsigned int bits;
};

static int
function_counters_table_init(struct function_counters_table* table,
	size_t n_elems);

static void
function_counters_table_destroy(struct function_counters_table* table);

/*
 * Return table element with given key.
 * 
 * If such element is not exits, allocate it.
 * 
 * On error, return ERR_PTR(error).
 */
static struct function_counters_elem*
function_counters_table_get(struct function_counters_table* table,
	void* function);


/*
 * Iterator through the table.
 */
struct function_counters_table_iter
{
	/* Public fields */
	/* If 0, then iterator shouldn't be used */
	int valid;
	/* Current element*/
	struct function_counters_elem* elem;
	
	/* Private fields */
	struct function_counters_table* table;
	int i;
};

/* 
 * Set iterator to the beginning of the table.
 * 
 * If table is empty, set 'valid' field of the iterator to 0.
 */
static void
function_counters_table_begin(struct function_counters_table* table,
	struct function_counters_table_iter* iter);

/* 
 * Advance iterator to the next element in the table.
 * 
 * If iterator point to the last element in the table,
 * set 'valid' field of the iterator to 0.
 */

static void function_counters_table_iter_next(struct function_counters_table_iter* iter);

/* Initialize arrays for kedr_base_interception_info */
static int
interception_info_init(struct kedr_base_interception_info* info,
    void* orig, int n_pre, int is_replaced, int n_post);

/* Add pre-function to the intermediate info */
static void
interception_info_add_pre(struct kedr_base_interception_info* info,
	void* pre_function);

/* Set replace function to the intermediate info */
static void
interception_info_add_pre(struct kedr_base_interception_info* info,
	void* pre_function);

/* Add post-function to the intermediate info */
static void
interception_info_set_replace(struct kedr_base_interception_info* info,
	void* replace_function);


static void
interception_info_destroy(struct kedr_base_interception_info* info);


/* Allocate appropriate amount of memory and combine the interception 
 * information from all fixed payload modules into a single array.
 * 
 * On error return ERR_PTR().
 */

static struct kedr_base_interception_info*
interception_info_array_create(void);

/* Free the combined interception info array */
static void
interception_info_array_free(struct kedr_base_interception_info* info_array);

/* Need when filling interception info array. */
static struct kedr_base_interception_info*
interception_info_array_find(struct kedr_base_interception_info* info_array,
    void* orig);

/* ================================================================ */
int
kedr_base_init(struct kedr_base_operations* ops)
{
	int result;
	KEDR_MSG(COMPONENT_STRING
		"initializing\n");

	kedr_base_ops = ops;
	/* Initialize the list of payloads */
	INIT_LIST_HEAD(&payload_list);
	payloads_are_used = 0;
	
	result = functions_map_init(&replaced_functions_map, 20);
    if(result) return result;
	
	return 0; 
}

void
kedr_base_destroy(void)
{
	functions_map_destroy(&replaced_functions_map);
	return;
}

/* =================Interface implementation================== */

/* 
 * On success, return 0 and interception info array is
 * info_array_current.
 * On error, return negative error code.
 * 
 * Should be called with mutex locked.
 */
static int
kedr_base_target_load_callback_internal(struct module* m)
{
	int result;
    
    struct kedr_base_interception_info* info_array;
	
	BUG_ON(payloads_are_used);
	
	result = payload_elem_fix_all(&payload_list);
	if(result) return result;
	
	info_array = interception_info_array_create();
	if(IS_ERR(info_array))
	{
		payload_elem_release_all(&payload_list);
		return PTR_ERR(info_array);
	}
	
	payloads_are_used = 1;
	
	payload_elem_load_callback_all(&payload_list, m);

    info_array_current = info_array;

	return 0;
}

/*
 * Fix all payloads and return array of functions with information
 * how them should be intercepted.
 * 
 * Last element in the array should contain NULL in 'orig' field.
 * 
 * On error, return ERR_PTR.
 * 
 * Returning array is freed by the callee
 * at kedr_target_unload_callback() call.
 */
const struct kedr_base_interception_info*
kedr_base_target_load_callback(struct module* m)
{
	/* 0 - return info_array_current, otherwise return ERR_PTR(result)*/
	int result;
	
	result = mutex_lock_killable(&base_mutex);

	if(result) return ERR_PTR(result);
	
	result = kedr_base_target_load_callback_internal(m);

	mutex_unlock(&base_mutex);
	
	return result ? ERR_PTR(result) : info_array_current;
}

/*
 * Make all payloads available to unload.
 */
void kedr_base_target_unload_callback(struct module* m)
{
	mutex_lock(&base_mutex);
	
	BUG_ON(!payloads_are_used);
	
	payload_elem_unload_callback_all(&payload_list, m);
	payloads_are_used = 0;
	interception_info_array_free(info_array_current);
	payload_elem_release_all(&payload_list);
	
	mutex_unlock(&base_mutex);
}

/* ================================================================ */
/* Implementation of public API                                     */
/* ================================================================ */

/* Should be executed with mutex locked */
static int 
kedr_payload_register_internal(struct kedr_payload *payload)
{
	int result = 0;
	struct payload_elem *elem_new = NULL;
	
	BUG_ON(payload == NULL);
	
	/* If there is a target module already watched for, do not allow
	 * to register another payload. */
	if (payloads_are_used)
	{
		pr_err("Fail to register new payload because KEDR functionality currently in use.");
		return -EBUSY;
	}

	if (payload_elem_find(payload, &payload_list) != NULL)
	{
        KEDR_MSG(COMPONENT_STRING
			"module \"%s\" attempts to register the same payload twice\n",
			module_name(payload->mod));
		return -EINVAL;
	}

	result = function_replacements_add_payload(&replaced_functions_map, payload);
	if(result)
	{
		pr_err("Fail to register payload because it replace function which already been replaced.");
		goto err_replacements;
	}
	
	result = payload_functions_use(payload);
	if(result)
	{
		goto err_functions_use;
	}
	
	
	KEDR_MSG(COMPONENT_STRING
		"registering payload from module \"%s\"\n",
		module_name(payload->mod));
	
	elem_new = kzalloc(sizeof(*elem_new), GFP_KERNEL);
	if (elem_new == NULL)
	{
		result = -ENOMEM;
		goto err_alloc_new_elem;
	}
		
	INIT_LIST_HEAD(&elem_new->list);
	elem_new->payload = payload;
	elem_new->is_used = 0;
	
	list_add_tail(&elem_new->list, &payload_list);
	
	return 0;

err_alloc_new_elem:
	payload_functions_unuse(payload);
err_functions_use:
	function_replacements_remove_payload(&replaced_functions_map, payload);
err_replacements:

	return result;
}

int 
kedr_payload_register(struct kedr_payload *payload)
{
	int result = 0;
	
	BUG_ON(payload == NULL);
	
	result = mutex_lock_killable(&base_mutex);
	if (result != 0)
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock base_mutex\n");
		return result;
	}

	result = kedr_payload_register_internal(payload);
	
	mutex_unlock(&base_mutex); 	
	return result;
}

void 
kedr_payload_unregister(struct kedr_payload *payload)
{
	struct payload_elem *elem;
	
	BUG_ON(payload == NULL);

	if (mutex_lock_killable(&base_mutex))
	{
		KEDR_MSG(COMPONENT_STRING
			"failed to lock base_mutex\n");
		return;
	}
	
	elem = payload_elem_find(payload, &payload_list);
	if (elem == NULL)
	{
		KEDR_MSG(COMPONENT_STRING
			"module \"%s\" attempts to unregister the payload "
			"that was never registered\n",
			module_name(payload->mod));
		goto out;
	}
	
	KEDR_MSG(COMPONENT_STRING
		"unregistering payload from module \"%s\"\n",
		module_name(payload->mod));
	
	list_del(&elem->list);
	kfree(elem);
	
	payload_functions_unuse(payload);

	function_replacements_remove_payload(&replaced_functions_map, payload);

out:
	mutex_unlock(&base_mutex);
	return;
}


/* ======Implementation of auxiliary functions======== */

/* Look for a given element in the list. */
static struct payload_elem* 
payload_elem_find(struct kedr_payload *payload, struct list_head* payload_list)
{
	struct payload_elem *elem;
	
	list_for_each_entry(elem, payload_list, list)
	{
		if(elem->payload == payload)
			return elem;
	}
	return NULL;
}


static int 
payload_elem_fix(struct payload_elem* elem)
{
	struct kedr_payload* payload = elem->payload;
	BUG_ON(elem->is_used);
	
	if((payload->mod != NULL) && (try_module_get(payload->mod) == 0))
	{
		return -EBUSY;
	}
	
	elem->is_used = 1;
	
	return 0;
}
static void 
payload_elem_release(struct payload_elem* elem)
{
	struct kedr_payload* payload = elem->payload;
	BUG_ON(!elem->is_used);
	
	if(payload->mod != NULL)
	{
		module_put(payload->mod);
	}
	
	elem->is_used = 0;
}

static int 
payload_elem_fix_all(struct list_head *elems)
{
	struct payload_elem* elem;
	list_for_each_entry(elem, elems, list)
	{
		int result = payload_elem_fix(elem);
		/* 
		 * Currently failure to fix even one payload is an error.
		 * 
		 * This makes 'is_used' field of the struct payload_elem
		 * unneccesary - it always has same value as global variable
		 * 'payloads_are_used'.
		 * 
		 * But may be in the future failure to fix one payload
		 * will not lead to the error.
		 */
		if(result)
		{
			struct payload_elem* elem_clear;
			list_for_each_entry(elem_clear, elems, list)
			{
				if(elem_clear == elem) break;
				payload_elem_release(elem_clear);
			}
			return result;
		}
	}
	return 0;
}
static void 
payload_elem_release_all(struct list_head *elems)
{
	struct payload_elem* elem;
	list_for_each_entry(elem, elems, list)
	{
		payload_elem_release(elem);
	}
}

static void 
payload_elem_load_callback_all(struct list_head *elems, struct module* m)
{
	struct payload_elem* elem;
	list_for_each_entry(elem, elems, list)
	{
		struct kedr_payload* payload = elem->payload;
		if(payload->target_load_callback)
			payload->target_load_callback(m);
	}
}
static void 
payload_elem_unload_callback_all(struct list_head *elems, struct module* m)
{
	struct payload_elem* elem;
	list_for_each_entry(elem, elems, list)
	{
		struct kedr_payload* payload = elem->payload;
		if(payload->target_unload_callback)
			payload->target_unload_callback(m);
	}
}

static int
payload_functions_use(struct kedr_payload* payload)
{
	int result = 0;

	if(kedr_base_ops == NULL) return 0;
	if(kedr_base_ops->function_use == NULL)	return 0;
	
	if(payload->pre_pairs)
	{
		struct kedr_pre_pair* pre_pair;
		
		for(pre_pair = payload->pre_pairs; pre_pair->orig != NULL; pre_pair++)
		{
			result = kedr_base_ops->function_use(kedr_base_ops,
                pre_pair->orig);
			if(result) break;
		}
		if(pre_pair->orig != NULL)
		{
			//Error
			if(kedr_base_ops->function_unuse)
				for(--pre_pair; (pre_pair - payload->pre_pairs) >= 0; pre_pair--)
				{
					kedr_base_ops->function_unuse(kedr_base_ops,
                        pre_pair->orig);
				}
			goto err_pre;
		}
	}

	if(payload->post_pairs)
	{
		struct kedr_post_pair* post_pair;

		for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
		{
			result = kedr_base_ops->function_use(kedr_base_ops,
                post_pair->orig);
			if(result) break;
		}
		if(post_pair->orig != NULL)
		{
			//Error
			if(kedr_base_ops->function_unuse)
				for(--post_pair; (post_pair - payload->post_pairs) >= 0; post_pair--)
				{
					kedr_base_ops->function_unuse(kedr_base_ops,
                        post_pair->orig);
				}
			goto err_post;
		}
	}

	if(payload->replace_pairs)
	{
		struct kedr_replace_pair* replace_pair;

		for(replace_pair = payload->replace_pairs; replace_pair->orig != NULL; replace_pair++)
		{
			result = kedr_base_ops->function_use(kedr_base_ops,
                replace_pair->orig);
			if(result) break;
		}
		if(replace_pair->orig != NULL)
		{
			//Error
			if(kedr_base_ops->function_unuse)
				for(--replace_pair; (replace_pair - payload->replace_pairs) >= 0; replace_pair--)
				{
					kedr_base_ops->function_unuse(kedr_base_ops,
                        replace_pair->orig);
				}
			goto err_replace;
		}
	}

	return 0;
	
err_replace:
	if(payload->post_pairs && kedr_base_ops->function_unuse)
	{
		struct kedr_post_pair* post_pair;

		for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
		{
			kedr_base_ops->function_unuse(kedr_base_ops,
                post_pair->orig);
		}
	}
err_post:
	if(payload->pre_pairs && kedr_base_ops->function_unuse)
	{
		struct kedr_pre_pair* pre_pair;

		for(pre_pair = payload->pre_pairs; pre_pair->orig != NULL; pre_pair++)
		{
			kedr_base_ops->function_unuse(kedr_base_ops,
                pre_pair->orig);
		}
	}
err_pre:
	BUG_ON(result == 0);
	return result;
}
static void
payload_functions_unuse(struct kedr_payload* payload)
{
	if(kedr_base_ops == NULL) return;
	if(kedr_base_ops->function_unuse == NULL) return;

	if(payload->replace_pairs)
	{
		struct kedr_replace_pair* replace_pair;

		for(replace_pair = payload->replace_pairs; replace_pair->orig != NULL; replace_pair++)
		{
			kedr_base_ops->function_unuse(kedr_base_ops,
                replace_pair->orig);
		}
	}

	if(payload->post_pairs)
	{
		struct kedr_post_pair* post_pair;

		for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
		{
			kedr_base_ops->function_unuse(kedr_base_ops,
                post_pair->orig);
		}
	}

	if(payload->pre_pairs)
	{
		struct kedr_pre_pair* pre_pair;
		for(pre_pair = payload->pre_pairs; pre_pair->orig != NULL; pre_pair++)
		{
			kedr_base_ops->function_unuse(kedr_base_ops,
                pre_pair->orig);
		}
	}
}


int
functions_map_init(struct functions_map* map, int n_elems)
{
	struct hlist_head* heads;
	size_t n_heads;
	unsigned int bits;
	
	n_heads = (n_elems * 10 + 6)/ 7;
	for(bits = 0; n_heads > 1; n_heads >>= 1)
	{
		bits++;
	}
	if(bits <= 1) bits = 1;
	
	n_heads = 1 << bits;
	
	heads = kzalloc(n_heads * sizeof(*heads), GFP_KERNEL);
	if(heads == NULL)
	{
		pr_err("functions_map_init: Failed to allocate functions map.");
		return -ENOMEM;
	}
	
	map->heads = heads;
	map->bits = bits;
	
	return 0;
}
void
functions_map_destroy(struct functions_map* map)
{
	int i;
	for(i = 0; i < map->bits; i++)
	{
		struct hlist_head* head = &map->heads[i];
		if(!hlist_empty(head))
		{
			pr_err("Destroying non-empty functions map");
			BUG();
		}
	}
	kfree(map->heads);
}

/* Return error if element already exist */
int
functions_map_add(struct functions_map* map, void* function)
{
	int i;
	struct functions_map_elem* map_elem;

	i = hash_ptr(function, map->bits);

	kedr_hlist_for_each_entry(map_elem, &map->heads[i], list)
	{
		if(map_elem->function == function) return -EBUSY;
	}
	
	map_elem = kmalloc(sizeof(*map_elem), GFP_KERNEL);
	if(map_elem == NULL)
	{
		pr_err("functions_map_add: Failed to allocate hash table entry.");
		return -ENOMEM;
	}
	map_elem->function = function;
	
	hlist_add_head(&map_elem->list, &map->heads[i]);

	return 0;

}
void
functions_map_remove(struct functions_map* map, void* function)
{
	int i;
	struct functions_map_elem* map_elem;

	i = hash_ptr(function, map->bits);

	kedr_hlist_for_each_entry(map_elem, &map->heads[i], list)
	{
		if(map_elem->function == function)
		{
			hlist_del(&map_elem->list);
			kfree(map_elem);
			return;
		}
	}

	BUG();
}

/* 
 * Return error if payload try to replace function which already replaced.
 */
int
function_replacements_add_payload(struct functions_map* replacement_map,
	struct kedr_payload* payload)
{
	if(payload->replace_pairs != NULL)
	{
		int error = 0;
		struct kedr_replace_pair* replace_pair;
		for(replace_pair = payload->replace_pairs;
			replace_pair->orig != NULL;
			replace_pair++)
		{
			error = functions_map_add(replacement_map, replace_pair->orig);
			if(error) break;
		}
		if(error)
		{
			for(--replace_pair;
				(replace_pair - payload->replace_pairs) >= 0;
				replace_pair--)
				functions_map_remove(replacement_map, replace_pair->orig);
			return error;
		}
	}
	
	return 0;
}

void
function_replacements_remove_payload(struct functions_map* replacement_map,
	struct kedr_payload* payload)
{
	if(payload->replace_pairs != NULL)
	{
		struct kedr_replace_pair* replace_pair;
		for(replace_pair = payload->replace_pairs;
			replace_pair->orig != NULL;
			replace_pair++)
		{
			functions_map_remove(replacement_map, replace_pair->orig);
		}
	}
}


int function_counters_table_init(struct function_counters_table* table, size_t n_elems)
{
	struct hlist_head* heads;
	size_t n_heads;
	unsigned int bits;
	
	n_heads = (n_elems * 10 + 6)/ 7;
	for(bits = 0; n_heads > 1; n_heads >>= 1)
	{
		bits++;
	}
	if(bits <= 1) bits = 1;
	
	n_heads = 1 << bits;
	
	heads = kzalloc(n_heads * sizeof(*heads), GFP_KERNEL);
	if(heads == NULL)
	{
		pr_err("function_counters_table_init: Failed to allocate functions table.");
		return -ENOMEM;
	}
	
	table->heads = heads;
	table->bits = bits;
	table->n_functions = 0;
	
	return 0;
}

void function_counters_table_destroy(struct function_counters_table* table)
{
	int i;
	size_t table_size = 1 << table->bits;
	
	for(i = 0; i < table_size; i++)
	{
		struct hlist_head* head = &table->heads[i];
		while(!hlist_empty(head))
		{
			struct function_counters_elem* elem =
				hlist_entry(head->first, struct function_counters_elem, list);
			hlist_del(&elem->list);
			kfree(elem);
		}
	}
	
	kfree(table->heads);
}

struct function_counters_elem*
function_counters_table_get(struct function_counters_table* table,
	void* function)
{
	int i;
	struct function_counters_elem* elem;

	i = hash_ptr(function, table->bits);

	kedr_hlist_for_each_entry(elem, &table->heads[i], list)
	{
		if(elem->function == function) return elem;
	}
	
	elem = kmalloc(sizeof(*elem), GFP_KERNEL);
	if(elem == NULL)
	{
		pr_err("function_counters_table_get: Failed to allocate hash table entry.");
		return ERR_PTR(-ENOMEM);
	}
	elem->function = function;
	elem->n_pre = 0;
	elem->is_replaced = 0;
	elem->n_post = 0;
	
	hlist_add_head(&elem->list, &table->heads[i]);
	table->n_functions++;

	return elem;
}

/* 
 * Set iterator to the beginning of the table.
 * 
 * If table is empty, set 'valid' field of the iterator to 0.
 */
void
function_counters_table_begin(struct function_counters_table* table,
	struct function_counters_table_iter* iter)
{
	int i;
	iter->table = table;
	for(i = 0; i < 1 << table->bits; i++)
	{
		struct hlist_head* head = &table->heads[i];
		if(hlist_empty(head)) continue;

		iter->i = i;
		iter->elem = hlist_entry(head->first, struct function_counters_elem, list);
		iter->valid = 1;
		
		return;
	}
	iter->valid = 0;
	return;
}

/* 
 * Advance iterator to the next element in the table.
 * 
 * If iterator point to the last element in the table,
 * set 'valid' field of the iterator to 0.
 */

void
function_counters_table_iter_next(struct function_counters_table_iter* iter)
{
	int i;
	size_t hash_table_size;

	BUG_ON(iter->valid == 0);
	
	if(iter->elem->list.next != NULL)
	{
		//iter->node_tmp = iter->node_tmp->next;
		iter->elem = hlist_entry(iter->elem->list.next,
			struct function_counters_elem, list);

		return;
	}
	hash_table_size = 1 << iter->table->bits;
	
	for(i = iter->i + 1; i < hash_table_size; i++)
	{
		struct hlist_head* head = &iter->table->heads[i];
		if(hlist_empty(head)) continue;

		iter->i = i;
		iter->elem = hlist_entry(head->first,
			struct function_counters_elem, list);
		iter->valid = 1;
		
		return;
	}
	iter->valid = 0;

	return;
}

int
interception_info_init(struct kedr_base_interception_info* info,
	void* orig, int n_pre, int is_replaced, int n_post)
{
	info->orig = orig;

	if(n_pre > 0)
	{
		info->pre =
			kmalloc((n_pre + 1) * sizeof(*info->pre), GFP_KERNEL);
		if(info->pre == NULL)
		{
			pr_err("Failed to allocate array of pre-functions.");
			return -ENOMEM;
		}
		/* initially array is empty*/
		info->pre[0] = NULL;
	}
	else
	{
		info->pre = NULL;
	}

	if(n_post > 0)
	{
		info->post =
			kmalloc((n_post + 1) * sizeof(*info->post), GFP_KERNEL);
		if(info->post == NULL)
		{
			pr_err("Failed to allocate array of post-functions.");
			kfree(info->pre);
			return -ENOMEM;
		}
		/* initially array is empty*/
		info->post[0] = NULL;
	}
	else
	{
		info->post = NULL;
	}

	/* not used */
	(void)is_replaced;
	
	info->replace = NULL;

	return 0;
}

/* Add pre-function to the interception info */
void
interception_info_add_pre(struct kedr_base_interception_info* info,
	void* pre_function)
{
	void** pre_elem;
	BUG_ON(info->pre == NULL);
	for(pre_elem = info->pre; *pre_elem != NULL; pre_elem++);
	
	*pre_elem = pre_function;
	*(pre_elem + 1) = NULL;
}


/* Add post-function to the interception info */
void
interception_info_add_post(struct kedr_base_interception_info* info,
	void* post_function)
{
	void** post_elem;
	BUG_ON(info->post == NULL);
	for(post_elem = info->post; *post_elem != NULL; post_elem++);
	
	*post_elem = post_function;
	*(post_elem + 1) = NULL;
}

/* Set replace function to the interception info */
void
interception_info_set_replace(struct kedr_base_interception_info* info,
	void* replace_function)
{
	BUG_ON(info->replace != NULL);
	info->replace = replace_function;
}

void
interception_info_destroy(struct kedr_base_interception_info* info)
{
	kfree(info->pre);
	kfree(info->post);
}

/* Allocate appropriate amount of memory and combine the interception 
 * information from all fixed payload modules into a single array.
 * 
 * On error return ERR_PTR().
 */

struct kedr_base_interception_info*
interception_info_array_create(void)
{
	int result;
    
	struct payload_elem *elem;
	struct kedr_base_interception_info* info_array = NULL;
	struct function_counters_table function_counters;
	struct function_counters_table_iter iter;
	int i;
	
	if (function_counters_table_init(&function_counters, 100))
	{
		return ERR_PTR(-ENOMEM);
	}
	/* Fill function_counters table from fixed payloads */
	list_for_each_entry(elem, &payload_list, list)
	{
		struct kedr_payload* payload;
		if(!elem->is_used) continue;
		
		payload = elem->payload;
		if(payload->pre_pairs != NULL)
		{
			struct kedr_pre_pair* pre_pair;
			for(pre_pair = payload->pre_pairs; pre_pair->orig != NULL; pre_pair++)
			{
				struct function_counters_elem* elem =
					function_counters_table_get(&function_counters,
						pre_pair->orig);
				if(IS_ERR(elem))
                {
                    result = PTR_ERR(elem);
                    goto err_update_counter;
                }
				elem->n_pre++;
			}
		}
		
		if(payload->replace_pairs != NULL)
		{
			struct kedr_replace_pair* replace_pair;
			for(replace_pair = payload->replace_pairs; replace_pair->orig != NULL; replace_pair++)
			{
				struct function_counters_elem* elem =
					function_counters_table_get(&function_counters,
						replace_pair->orig);
				if(IS_ERR(elem))
                {
                    result = PTR_ERR(elem);
                    goto err_update_counter;
                }

				BUG_ON(elem->is_replaced);
				elem->is_replaced = 1;
			}
		}

		if(payload->post_pairs != NULL)
		{
			struct kedr_post_pair* post_pair;
			for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
			{
				struct function_counters_elem* elem =
					function_counters_table_get(&function_counters,
						post_pair->orig);
				if(IS_ERR(elem))
                {
                    result = PTR_ERR(elem);
                    goto err_update_counter;
                }
				elem->n_post++;
			}
		}
	}
	
	info_array = kmalloc((function_counters.n_functions + 1) * sizeof(*info_array), GFP_KERNEL);
	
	if(info_array == NULL)
	{
		pr_err("interception_info_array_create: Fail to allocate array.");
        result = -ENOMEM;
		goto err_allocate_array;
	}
	
	for (function_counters_table_begin(&function_counters, &iter), i = 0;
		iter.valid;
		function_counters_table_iter_next(&iter), i++)
	{
		/* initialize interception info for given function */
		result = interception_info_init(&info_array[i],
            iter.elem->function, iter.elem->n_pre,
            iter.elem->is_replaced, iter.elem->n_post);
		if(result)
		{
			goto err_interception_info;
		}
    }
	
	BUG_ON(i != function_counters.n_functions);
	info_array[i].orig = NULL;

	/* Fill interception info for every function which is used by payloads */
	list_for_each_entry(elem, &payload_list, list)
	{
		struct kedr_payload* payload;
		if(!elem->is_used) continue;
		
		payload = elem->payload;
		if(payload->pre_pairs != NULL)
		{
			struct kedr_pre_pair* pre_pair;
			for(pre_pair = payload->pre_pairs; pre_pair->orig != NULL; pre_pair++)
			{
				struct kedr_base_interception_info* info_elem =
					interception_info_array_find(info_array, pre_pair->orig);
				BUG_ON(info_elem == NULL);
				interception_info_add_pre(info_elem, pre_pair->pre);
			}
		}
		
		if(payload->replace_pairs != NULL)
		{
			struct kedr_replace_pair* replace_pair;
			for(replace_pair = payload->replace_pairs; replace_pair->orig != NULL; replace_pair++)
			{
				struct kedr_base_interception_info* info_elem =
					interception_info_array_find(info_array, replace_pair->orig);
				BUG_ON(info_elem == NULL);
				interception_info_set_replace(info_elem, replace_pair->replace);
			}
		}

		if(payload->post_pairs != NULL)
		{
			struct kedr_post_pair* post_pair;
			for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
			{
				struct kedr_base_interception_info* info_elem =
					interception_info_array_find(info_array, post_pair->orig);
				BUG_ON(info_elem == NULL);
				interception_info_add_post(info_elem, post_pair->post);
			}
		}
	}
	
	function_counters_table_destroy(&function_counters);
	
	return info_array;

err_interception_info:
	/* Free all previously allocated interception info*/
	for(--i; i >= 0; i--)
	{
		interception_info_destroy(&info_array[i]);
	}
	/* And free array itself */
	kfree(info_array);
err_allocate_array:
err_update_counter:
	function_counters_table_destroy(&function_counters);

	return ERR_PTR(result);
}


/* Free the combined interception info array */
void
interception_info_array_free(struct kedr_base_interception_info* info_array)
{
	struct kedr_base_interception_info* info;
	if(info_array == NULL) return;//nothing to do
	
	for(info = info_array; info->orig != NULL; info++)
	{
		interception_info_destroy(info);
	}

	kfree(info_array);

	info_array = NULL;
}


struct kedr_base_interception_info*
interception_info_array_find(struct kedr_base_interception_info* info_array,
    void* orig)
{
    struct kedr_base_interception_info* elem;
    for(elem = info_array; elem->orig != NULL; elem++)
    {
        if(elem->orig == orig) return elem;
    }
    return NULL;
}
/* ================================================================ */
