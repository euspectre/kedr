/*
 * The "base" component of KEDR system. 
 * "kedr-base" keeps a registry of payload modules and provides interface 
 * for them to register / unregister themselves. 
 * It also provides convenience functions for payload modules and the interface 
 * that "kedr-controller" uses during the instrumentation of a target module.
 */

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
 * After some intermediate errors it is difficult to return KEDR to
 * reasonable consistent state.
 * In such cases ALL useful functionality of the KEDR will be disabled.
 * The only way to restore this functionality - unload KEDR module and load it again.
 * recovery
 * The same approach will be used for errors, which are recoverable for KEDR,
 * but hardly recoverable for user.
 * 
 * When this flag is set, all usefull functionality of the KEDR will be disable:
 * 
 * - one cannot register any new payload or functions_support,
 * - but one can "unregister" any payload or functions_support
 *    (even those, which wasn't registered). Of course, this action really do nothing.
 *  -kedr_base_target_load_callback() will always fail.
 */

static int is_disabled = 0;

/*
 * Set 'is_disable' flag and free all resources.
 * 
 * Function may be called in any internally consistent state:
 * -list of payload registered exists and for every payload element
 *  ('is_used' != 0) is true only when payload is fixed
 * -list of functions_support elements exists and for every element
 *  ('n_usage' != 0) is true only when element is fixed.
 * */
static void disable_all_functionality(void);

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

/* 
 * All registered 'kedr_functions_support's are organized into list.
 * This is element of this list.
 */
struct functions_support_elem
{
	struct list_head list;
	struct kedr_functions_support* functions_support;
	
	/*
	 * Refcount of used functions from this set.
	 * 
	 * Functions support elements with 'n_usage' != 0
	 * cannot be unregistered.
	 */
	
	int n_usage;
};

static struct functions_support_elem*
functions_support_elem_find(struct kedr_functions_support* functions_support,
	struct list_head* functions_support_list);

/*
 * Increment usage counter of the functions_support element.
 * 
 * Also, fix element at the first usage. If this operation failed
 * return error.
 */
static int
functions_support_elem_use(struct functions_support_elem* support_elem);

/* 
 * Decrement usage counter of the functions_support element.
 * 
 * Also, release element at the last usage.
 */
static void
functions_support_elem_unuse(struct functions_support_elem* support_elem);

/* 
 * Information about functions which is available for replace/intercept
 * is organized into hash table.
 * 
 * This table is used for verify consraints on functions_support and 
 * payloads registrations:
 * 
 * -payloads cannot register more than one replace function for original function.
 * -if function is used by at least one payload, the last(!) support element, which
 * implement intermediate function for this functuin, cannot be unregistered.
 * 
 * Also, this table is used for track function to its support element,
 * its intermediate replacement function and its intermediate info.
 * 
 * This is an element of the list of the element of this table.
 */

struct function_info_support_elem
{
	struct list_head list;
	struct functions_support_elem* support_elem;
};
struct function_info_elem
{
	/* hash table organization */
	struct hlist_node list;
	/* address of the original functions(key in the hash table) */
	void* orig;
	/* 
	 * List of the functions_support_elem'ents, which supported this function.
	 */
	struct list_head support_elems;
	
	/* 
	 * Number of usage of this function by payloads.
	 * 
	 * If n_usage not 0 and 'support_elems' contain only one element,
	 * this element should be fixed (functions_support_elem_use()).
	 */
	int n_usage;
	/* 
	 * Whether use_support() was called for this function.
	 * 
	 * When this flag is set, first element in the support_elems is fixed
	 * (functions_support_elem_use()).
	 */
	int is_used_support;

	/* 
	 * Whether this functions is replaced by some payload 
	 * (each function may be replaced at most one payload).
	 */
	int is_replaced;

	/* Intermediate replacement for this function
	 * and info for this replacement.
	 * 
	 * These fields make a sence only after function_info_use_support() call
	 * and until function_info_unuse_support().
	 * */
	void* intermediate;
	struct kedr_intermediate_info* intermediate_info;
};

/* Initialize function info element as not supported */
static void function_info_elem_init(struct function_info_elem* info_elem, void* orig);

/* Destroy function info element. At this stage it should be not supported. */
static void function_info_elem_destroy(struct function_info_elem* info_elem);
/* Destroy function info element. For disable_all_functionality(). */
static void function_info_elem_destroy_clean(struct function_info_elem* info_elem);

/* Mark function as supported by 'supp_elem'*/
static int
function_info_elem_add_support(struct function_info_elem* info_elem,
	struct functions_support_elem* support_elem);
/*
 * Mark function as not supported by 'supp_elem'.
 * 
 * Note: This function may fail!
 */
static int function_info_elem_remove_support(struct function_info_elem* info_elem,
	struct functions_support_elem* support_elem);

/* 
 * Return not 0 if function is supported.
 * 
 * Note: Not supported functions may exist only in intermediate state.
 * E.g., last support was just removed or function_info_elem entry is just
 * created and add_supp() was failed.
 * */
static int function_info_elem_is_supported(struct function_info_elem* info_elem);

/* 
 * Increment usage refcounting of the function.
 * 
 * Function may fail in case when support for this function is failed to fix.
 */
static int function_info_elem_use(struct function_info_elem* info_elem);
/* Decrement usage refcounting of the function */
static void function_info_elem_unuse(struct function_info_elem* info_elem);
/*
 * Choose one support of the function, and mark it as used.
 * 
 * This function is intended to call when we need to create
 * concrete table of concrete replacement functions.
 * 
 * After this call fields 'intermediate' and 'intermediate_info'
 * become sensible.
 * 
 * Also, after this operation, adding and removing support for
 * function will fail(will return -EBUSY).
 */
static int function_info_elem_use_support(struct function_info_elem* info_elem);
/*
 * Release choosen support of the function.
 * 
 * After this call fields 'intermediate' and 'intermediate_info'
 * have no sence.
 * 
 * Adding and removing support became available after this call.
 */
static void function_info_elem_unuse_support(struct function_info_elem* info_elem);

struct function_info_table
{
	struct hlist_head* heads;
	unsigned int bits;
};

/*
 * Initialize function info table
 * which is optimized to work with 'n_elems' elements.
 * 
 * Return 0 on success or negative error code on error.
 */
static int
function_info_table_init(struct function_info_table* table,
	size_t n_elems);

/*
 * Destroy function_info_elem table.
 * 
 * Table should be empty at this stage.
 */
static void
function_info_table_destroy(struct function_info_table* table);

/* Destroy function info table. For disable_all_functionality(). */
static void
function_info_table_destroy_clean(struct function_info_table* table);

/*
 * Return element from the function info table with given key.
 * If element with such key is not exist, create it as not supported.
 * 
 * Return ERR_PTR(error) on error.
 * 
 * Note: Unsupported function may exist only in intermediate state.
 */
static struct function_info_elem*
function_info_table_get(struct function_info_table* table, void* orig);

/*
 * Find element with given key in the table and return it.
 * 
 * If element is not exist, return NULL.
 */

static struct function_info_elem*
function_info_table_find(struct function_info_table* table, void* orig);

/*
 * Remove given element from the table.
 */
static void
function_info_table_remove(struct function_info_table* table,
	struct function_info_elem* info_elem);

/* Add statistic about payload to the table */
static int
function_info_table_add_payload(struct function_info_table* table,
	struct kedr_payload* payload);
/* Remove statistic about payload from the table */
static void
function_info_table_remove_payload(struct function_info_table* table,
	struct kedr_payload* payload);

/* Add information about functions from functions_support.*/
static int
function_info_table_add_support(struct function_info_table* table,
	struct functions_support_elem* support_elem);
/*
 * Remove information about functions from functions_support.
 * 
 * NOTE: May disable all KEDR functionality - so one need to verify
 * 'is_disabled' flag after call of this function.
 */
static int
function_info_table_remove_support(struct function_info_table* table,
	struct functions_support_elem* support_elem);


/* ================= Global data =================== */

/* List of currently registered payloads */
static struct list_head payload_list;

/*
 * Nonzero if payloads are used now (target module is loaded.).
 */
static int payloads_are_used;
/* The list of currently loaded functions support*/
static struct list_head functions_support_list;

/*
 * Table with information about all available functions for payloads.
 * 
 * Used for many purposes.
 */
static struct function_info_table functions;

/* 
 * The combined replacement table (from all payload modules).
 * 
 * Need to store for freeing when target module is unloaded.
 */

static struct kedr_replace_real_pair* combined_repl_table;

/* A mutex to protect access to the global data of kedr-base*/
DEFINE_MUTEX(base_mutex);

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
	void* func;
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
	void* func);


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

/* Initialize arrays for kedr_intermediate_info */
static int
intermediate_info_init(struct kedr_intermediate_info* info,
	int n_pre, int is_replaced, int n_post);

/* Add pre-function to the intermediate info */
static void
intermediate_info_add_pre(struct kedr_intermediate_info* info,
	void* pre_function);

/* Set replace function to the intermediate info */
static void
intermediate_info_add_pre(struct kedr_intermediate_info* info,
	void* pre_function);

/* Add post-function to the intermediate info */
static void
intermediate_info_set_replace(struct kedr_intermediate_info* info,
	void* replace_function);


static void
intermediate_info_destroy(struct kedr_intermediate_info* info);


/* Free the combined replacement table*/
static void
repl_table_free(struct kedr_replace_real_pair* repl_table);

/* Allocate appropriate amount of memory and combine the replacement 
 * tables from all fixed payload modules into a single 'table'.
 * 
 * On error return NULL.
 * 
 * Note: ERR_PTR()...
 */

static struct kedr_replace_real_pair*
repl_table_create(void);

/* ================================================================ */
int
kedr_base_init(void)
{
	int result;
	KEDR_MSG(COMPONENT_STRING
		"initializing\n");

	/* Initialize the list of payloads */
	INIT_LIST_HEAD(&payload_list);
	payloads_are_used = 0;
	
	INIT_LIST_HEAD(&functions_support_list);
	
	result = function_info_table_init(&functions, 100);
	if(result) return result;
	
	is_disabled = 0;

	return 0; 
}

void
kedr_base_destroy(void)
{
	if(!is_disabled)
		function_info_table_destroy(&functions);
	return;
}

/* =================Interface implementation================== */

/* 
 * On success, return 0 and table is combined_repl_table.
 * On error, return negative error code.
 * 
 * Should be called with mutex locked.
 */
static int
kedr_base_target_load_callback_internal(struct module* m)
{
	int result;
	
	if(is_disabled) return -EINVAL;
	
	BUG_ON(payloads_are_used);
	
	result = payload_elem_fix_all(&payload_list);
	if(result) return result;
	
	combined_repl_table = repl_table_create();
	if(IS_ERR(combined_repl_table))
	{
		payload_elem_release_all(&payload_list);
		return PTR_ERR(combined_repl_table);
	}
	
	payloads_are_used = 1;
	
	payload_elem_load_callback_all(&payload_list, m);

	return 0;
}

/*
 * Fix all payloads and return list of functions to replace.
 * 
 * Last element in this list contain NULL in 'orig' field.
 * 
 * On error, return NULL.
 *
 * TODO: It make sence to return ERR_PTR on error.
 * 
 * Returning list is freed by the callee
 * at kedr_target_unload_callback() call.
 */
struct kedr_replace_real_pair*
kedr_base_target_load_callback(struct module* m)
{
	/* 0 - return combined_repl_table, otherwise return ERR_PTR(result)*/
	int result;
	
	result = mutex_lock_killable(&base_mutex);

	if(result) return ERR_PTR(result);
	
	result = kedr_base_target_load_callback_internal(m);

	mutex_unlock(&base_mutex);
	
	return result ? ERR_PTR(result) : combined_repl_table;
}

/*
 * Make all payloads available to unload.
 */
void kedr_base_target_unload_callback(struct module* m)
{

	if(mutex_lock_killable(&base_mutex))
	{
		//TODO: Is this correct to silently return on error?
		return;
	}
	
	BUG_ON(!payloads_are_used);
	/* When payloads are used, one cannot disable KEDR functionality */
	BUG_ON(is_disabled);
	
	payload_elem_unload_callback_all(&payload_list, m);
	payloads_are_used = 0;
	repl_table_free(combined_repl_table);
	payload_elem_release_all(&payload_list);
	
	mutex_unlock(&base_mutex);
}

/* ================================================================ */
/* Implementation of public API                                     */
/* ================================================================ */

static int
kedr_functions_support_register_internal(struct kedr_functions_support* functions_support)
{
	int result;
	struct functions_support_elem* support_elem_new =
		kmalloc(sizeof(*support_elem_new), GFP_KERNEL);
		
	if(support_elem_new == NULL)
	{
		pr_err("kedr_functions_support_register: Failed to allocate structure for new support element.");
		return -ENOMEM;
	}
	support_elem_new->functions_support = functions_support;
	support_elem_new->n_usage = 0;
	
	//pr_info("Functions support element %p is created.",
	//	support_elem_new);

	
	result = function_info_table_add_support(&functions, support_elem_new);
	if(result)
	{
		// TODO: some information should be printed here according to error.
		kfree(support_elem_new);
		return result;
	}
	
	list_add(&support_elem_new->list, &functions_support_list);

	return 0;
}

/*
 * Register kedr support for some functions set.
 * 
 * Functions sets from different registrations shouldn't intercept
 * with each other.
 */
int kedr_functions_support_register(struct kedr_functions_support* functions_support)
{
	int result;
	
	result = mutex_lock_killable(&base_mutex);
	if(result) return result;
	
	result = (!is_disabled)
		? kedr_functions_support_register_internal(functions_support)
		: -EINVAL;

	mutex_unlock(&base_mutex);
	return result;
}

/* Should be exesuted with mutex locked */
static int
kedr_functions_support_unregister_internal(struct kedr_functions_support* functions_support)
{
	int result;
	struct functions_support_elem* support_elem =
		functions_support_elem_find(functions_support, &functions_support_list);
	
	if(support_elem == NULL)
	{
		pr_err("kedr_functions_support_unregister: Attempt to unregister support which wasn't register.");
		return -EINVAL;
	}
	
	if(support_elem->n_usage)
	{
		pr_err("kedr_functions_support_unregister: Attempt to unregister support which is used now.");
		return -EBUSY;
	}
	
	result = function_info_table_remove_support(&functions, support_elem);
	if(result)
	{
		if(is_disabled) return result;//simple chain error
		pr_err("kedr_functions_support_unregister: Failed to unregister functions support.");
		return result;
	}
	
	list_del(&support_elem->list);
	kfree(support_elem);
	//pr_info("Functions support element %p is destroyed.",
	//	support_elem);

	
	return 0;
}

int kedr_functions_support_unregister(struct kedr_functions_support* functions_support)
{
	int result = 0;
	
	result = mutex_lock_killable(&base_mutex);
	if(result) return result;
	
	result = !is_disabled
		? kedr_functions_support_unregister_internal(functions_support)
		: 0;
		
	
	mutex_unlock(&base_mutex);

	return result;
}

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

	result = function_info_table_add_payload(&functions, payload);
	if(result)
	{
		//pr_err("Fail to register payload because it replace function which already been replaced.");
		goto err_info_add;
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
	function_info_table_remove_payload(&functions, payload);
err_info_add:

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

	result = (!is_disabled)
		? kedr_payload_register_internal(payload)
		: -EINVAL;

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
	
	if(is_disabled) goto out;

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
	
	function_info_table_remove_payload(&functions, payload);

out:
	mutex_unlock(&base_mutex);
	return;
}


/* ======Implementation of auxiliary functions======== */

void disable_all_functionality(void)
{
	is_disabled = 1;
	function_info_table_destroy_clean(&functions);
	
	/* Clean list of registered payload and release modules when it need */
	while(!list_empty(&payload_list))
	{
		struct payload_elem* elem =
			list_first_entry(&payload_list, struct payload_elem, list);
		if(elem->is_used)
		{
			if(elem->payload->mod != NULL)
				module_put(elem->payload->mod);
		}
		list_del(&elem->list);
		kfree(elem);
	}
	/* Clean list of registered functions supports and release modules when it need */
	while(!list_empty(&functions_support_list))
	{
		struct functions_support_elem* support_elem =
			list_first_entry(&functions_support_list, struct functions_support_elem, list);
		if(support_elem->n_usage != 0)
		{
			if(support_elem->functions_support->mod != NULL)
				module_put(support_elem->functions_support->mod);
		}
		list_del(&support_elem->list);
		kfree(support_elem);
	}

}

/* Look for a given element in the list. */
struct payload_elem* 
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


int payload_elem_fix(struct payload_elem* elem)
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
void payload_elem_release(struct payload_elem* elem)
{
	struct kedr_payload* payload = elem->payload;
	BUG_ON(!elem->is_used);
	
	if(payload->mod != NULL)
	{
		module_put(payload->mod);
	}
	
	elem->is_used = 0;
}

int payload_elem_fix_all(struct list_head *elems)
{
	struct payload_elem* elem;
	list_for_each_entry(elem, elems, list)
	{
		int result = payload_elem_fix(elem);
		/* 
		 * Currently failure to fix even one payload is an error.
		 * 
		 * This make 'is_used' field of the struct payload_elem
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
void payload_elem_release_all(struct list_head *elems)
{
	struct payload_elem* elem;
	list_for_each_entry(elem, elems, list)
	{
		payload_elem_release(elem);
	}
}

void payload_elem_load_callback_all(struct list_head *elems, struct module* m)
{
	struct payload_elem* elem;
	list_for_each_entry(elem, elems, list)
	{
		struct kedr_payload* payload = elem->payload;
		if(payload->target_load_callback)
			payload->target_load_callback(m);
	}
}
void payload_elem_unload_callback_all(struct list_head *elems, struct module* m)
{
	struct payload_elem* elem;
	list_for_each_entry(elem, elems, list)
	{
		struct kedr_payload* payload = elem->payload;
		if(payload->target_unload_callback)
			payload->target_unload_callback(m);
	}
}

static struct functions_support_elem*
functions_support_elem_find(struct kedr_functions_support* functions_support,
	struct list_head* functions_support_list)
{
	struct functions_support_elem* support_elem;
	list_for_each_entry(support_elem, functions_support_list, list)
	{
		if(support_elem->functions_support == functions_support) return support_elem;
	}
	return NULL;
}

int
functions_support_elem_use(struct functions_support_elem* support_elem)
{
	struct kedr_functions_support* functions_support = support_elem->functions_support;
	if((support_elem->n_usage == 0) && (functions_support->mod != NULL))
		if(try_module_get(functions_support->mod) == 0)
		{
			pr_err("functions_support_elem_use: Fail to fix functions support.");
			// TODO: error code should be changed to more suited one.
			return -EBUSY;
		}
	support_elem->n_usage++;
	//pr_info("Refcounting of function_support element %p is increased.", support_elem);
	return 0;
}

void
functions_support_elem_unuse(struct functions_support_elem* support_elem)
{
	struct kedr_functions_support* functions_support = support_elem->functions_support;
	
	support_elem->n_usage--;
	if((support_elem->n_usage == 0) && (functions_support->mod != NULL))
	{
		module_put(functions_support->mod);
	}
	//pr_info("Refcounting of function_support element %p is decreased.", support_elem);

}

/* Initialize function as not supported */
void function_info_elem_init(struct function_info_elem* info_elem, void* orig)
{
	info_elem->orig = orig;
	
	INIT_LIST_HEAD(&info_elem->support_elems);
	
	info_elem->n_usage = 0;
	info_elem->is_used_support = 0;
	
	info_elem->is_replaced = 0;
	/* shouldn't be used */
	info_elem->intermediate = NULL;
	info_elem->intermediate_info = NULL;
	
	//pr_info("Function info element %p is created(function is %p).",
	//	info_elem, orig);
	
}

/* Destroy function info. At this stage it should be not supported. */
void function_info_elem_destroy(struct function_info_elem* info_elem)
{
	BUG_ON(!list_empty(&info_elem->support_elems));
	BUG_ON(info_elem->n_usage);
	BUG_ON(info_elem->is_used_support);
	BUG_ON(info_elem->is_replaced);
	/* nothing to do */
	//pr_info("Function info element %p is destroyed(function is %p).",
	//	info_elem, info_elem->orig);

}
/* Destroy function info. For disable_all_functionality(). */
static void function_info_elem_destroy_clean(struct function_info_elem* info_elem)
{
	BUG_ON(info_elem->is_used_support);
	while(!list_empty(&info_elem->support_elems))
	{
		kfree(list_first_entry(&info_elem->support_elems, struct function_info_support_elem, list));
	}
}

/* Mark function as supported by 'supp_elem'*/
static int
function_info_elem_add_support(struct function_info_elem* info_elem,
	struct functions_support_elem* support_elem)
{
	struct list_head* support_elems = &info_elem->support_elems;
	struct function_info_support_elem* info_support_elem;
	
	if(info_elem->is_used_support) return -EBUSY;
	
	info_support_elem = kmalloc(sizeof(*info_support_elem), GFP_KERNEL);
	if(info_support_elem == NULL)
	{
		pr_err("Fail to allocate functions support element for function info element.");
		return -ENOMEM;
	}
	
	info_support_elem->support_elem = support_elem;

	if(info_elem->n_usage)
	{
		/* If function currently supported only by one element, release this element*/
		if((support_elems->next == support_elems->prev)
			&& (support_elems->next != support_elems))
		{
			struct function_info_support_elem* info_support_elem_only =
				list_entry(support_elems->next, struct function_info_support_elem, list);
			functions_support_elem_unuse(info_support_elem_only->support_elem);
		}
	}

	list_add_tail(&info_support_elem->list, support_elems);
	
	return 0;
}
/*
 * Mark function as not supported by 'supp_elem'.
 * 
 * Note: This function may fail!
 */
static int function_info_elem_remove_support(struct function_info_elem* info_elem,
	struct functions_support_elem* support_elem)
{
	struct function_info_support_elem* info_support_elem;
	int found = 0;
	list_for_each_entry(info_support_elem, &info_elem->support_elems, list)
	{
		if(info_support_elem->support_elem == support_elem)
		{
			found = 1;
			break;
		}
	}
	BUG_ON(!found);
	/* 'elem' - list element for remove */
	if(info_elem->is_used_support && (info_support_elem->list.prev == &info_elem->support_elems))
	{
		// TODO: Is this situation reachable?
		return -EBUSY;
	}
	if(info_elem->n_usage)
	{
		/* 
		 * If we remove one of two element from the list,
		 * the other element should be fixed.
		 */
		if(info_support_elem->list.next != &info_elem->support_elems)
		{
			struct function_info_support_elem* info_support_elem_next =
				list_entry(info_support_elem->list.next,
					struct function_info_support_elem, list);
			if(info_support_elem_next->list.next == info_support_elem->list.prev)
			{
				if(functions_support_elem_use(info_support_elem_next->support_elem))
					goto err_use;
			}
		}
		else if(info_support_elem->list.prev != &info_elem->support_elems)
		{
			struct function_info_support_elem* info_support_elem_prev =
				list_entry(info_support_elem->list.prev,
					struct function_info_support_elem, list);
			if(info_support_elem_prev->list.prev == info_support_elem->list.next)
			{
				if(functions_support_elem_use(info_support_elem_prev->support_elem))
					goto err_use;
			}
		}

	}

	list_del(&info_support_elem->list);
	kfree(info_support_elem);
	
	return 0;
err_use:

	return -EBUSY;

}

/* 
 * Return not 0 if function is supported.
 * 
 * Note: Not supported functions may exist only in intermediate state.
 * E.g., last support was just removed or function info entry is just
 * created and add_support() was failed.
 * */
static int function_info_elem_is_supported(struct function_info_elem* info_elem)
{
	return !list_empty(&info_elem->support_elems);
}

/* 
 * Increment usage refcounting of the function.
 * 
 * Function may fail in case when support for this function is failed to fix.
 */
static int function_info_elem_use(struct function_info_elem* info_elem)
{
	if(info_elem->n_usage == 0)
	{
		struct list_head* support_elems = &info_elem->support_elems;
		/* If only one element in the supported list - fix it */
		if((support_elems->next == support_elems->prev)
			&& (support_elems->next != support_elems))
		{
			int result;
			struct function_info_support_elem* info_support_elem =
				list_entry(support_elems->next,
					struct function_info_support_elem, list);
			result = functions_support_elem_use(info_support_elem->support_elem);
			if(result) return result;
		}
	}
	info_elem->n_usage++;
	
	//pr_info("Refcounting of function_info element %p is increased.", info_elem);
	return 0;
}
/* Decrement usage refcounting of the function */
static void function_info_elem_unuse(struct function_info_elem* info_elem)
{
	info_elem->n_usage--;

	if(info_elem->n_usage == 0)
	{
		struct list_head* support_elems = &info_elem->support_elems;
		/* If only one element in the supported list - release it */
		if((support_elems->next == support_elems->prev)
			&& (support_elems->next != support_elems))
		{
			struct function_info_support_elem* info_support_elem =
				list_entry(support_elems->next,
					struct function_info_support_elem, list);
			functions_support_elem_unuse(info_support_elem->support_elem);
		}
	}
	//pr_info("Refcounting of function_info element %p is decreased.", info_elem);
}
/*
 * Choose one support of the function, and mark it as used.
 * 
 * This function is intended to call when we need to create
 * concrete table of concrete replacement functions.
 * 
 * After this call fields 'intermediate' and 'intermediate_info'
 * become sensible.
 * 
 * Also, after this operation, adding and removing support for
 * function will fail(will return -EBUSY).
 */
static int function_info_elem_use_support(struct function_info_elem* info_elem)
{
	int result;
	struct functions_support_elem* support_elem;
	struct kedr_intermediate_impl* impl;
	
	BUG_ON(info_elem->is_used_support);
	
	BUG_ON(list_empty(&info_elem->support_elems));
	/* Use support from the first element in the list */
	support_elem = list_first_entry(&info_elem->support_elems,
		struct function_info_support_elem, list)->support_elem;
	
	result = functions_support_elem_use(support_elem);
	if(result) return result;
	
	info_elem->is_used_support = 1;
	
	/* look for intermediate function and info for it */
	for(impl = support_elem->functions_support->intermediate_impl; impl->orig != NULL; impl++)
	{
		if(impl->orig == info_elem->orig)
		{
			info_elem->intermediate = impl->intermediate;
			info_elem->intermediate_info = impl->info;
			return 0;
		}
	}
	BUG();
}
/*
 * Release choosen support of the function.
 * 
 * After this call fields 'intermediate' and 'intermediate_info'
 * have no sence.
 * 
 * Adding and removing support became available after this call.
 */
static void function_info_elem_unuse_support(struct function_info_elem* info_elem)
{
	struct functions_support_elem* support_elem;
	
	BUG_ON(!info_elem->is_used_support);
	
	BUG_ON(list_empty(&info_elem->support_elems));
	/* Release support from the first element in the list */
	support_elem = list_first_entry(&info_elem->support_elems,
		struct function_info_support_elem, list)->support_elem;
	
	functions_support_elem_unuse(support_elem);
	
	info_elem->is_used_support = 0;
	/* Next fields shouldn't be used after this function call */
	info_elem->intermediate = NULL;
	info_elem->intermediate_info = NULL;
}


/*
 * Initialize function info table
 * which is optimized to work with 'n_elems' elements.
 * 
 * Return 0 on success and negative error code on error.
 */
int
function_info_table_init(struct function_info_table* table,
	size_t n_elems)
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
		pr_err("function_info_table_init: Failed to allocate functions info table.");
		return -ENOMEM;
	}
	
	table->heads = heads;
	table->bits = bits;
	
	return 0;
}

/*
 * Destroy function info table.
 * 
 * Table should be empty at this stage.
 */
void
function_info_table_destroy(struct function_info_table* table)
{
	int i;
	for(i = 0; i < table->bits; i++)
	{
		struct hlist_head* head = &table->heads[i];
		if(!hlist_empty(head))
		{
			pr_err("Destroying non-empty function info table");
			BUG();
		}
	}
	kfree(table->heads);
}

/* Destroy function info table. For disable_all_functionality(). */
void
function_info_table_destroy_clean(struct function_info_table* table)
{
	int i;
	for(i = 0; i < table->bits; i++)
	{
		struct hlist_head* head = &table->heads[i];
		while(!hlist_empty(head))
		{
			struct function_info_elem* info_elem =
				hlist_entry(head->first, struct function_info_elem, list);
			function_info_elem_destroy_clean(info_elem);
			hlist_del(&info_elem->list);
			kfree(info_elem);
		}
	}
	kfree(table->heads);
}


/*
 * Return element from the function info table with given key.
 * If element with such key is not exist, create it as not supported.
 * 
 * Return ERR_PTR(error) on error.
 * 
 * Note: Unsupported function may exist only in intermediate state.
 */
struct function_info_elem*
function_info_table_get(struct function_info_table* table, void* orig)
{
	int i;
	struct hlist_node* node_tmp;
	struct function_info_elem* info_elem;

	i = hash_ptr(orig, table->bits);

	hlist_for_each_entry(info_elem, node_tmp, &table->heads[i], list)
	{
		if(info_elem->orig == orig) return info_elem;
	}
	
	info_elem = kmalloc(sizeof(*info_elem), GFP_KERNEL);
	if(info_elem == NULL)
	{
		pr_err("function_info_table_add: Failed to allocate hash table entry.");
		return ERR_PTR(-ENOMEM);
	}
	function_info_elem_init(info_elem, orig);
	
	hlist_add_head(&info_elem->list, &table->heads[i]);

	return info_elem;
}

/*
 * Find element with given key in the table and return it.
 * 
 * If element is not exist, return NULL.
 */

struct function_info_elem*
function_info_table_find(struct function_info_table* table, void* orig)
{
	int i;
	struct hlist_node* node_tmp;
	struct function_info_elem* info_elem;

	i = hash_ptr(orig, table->bits);

	hlist_for_each_entry(info_elem, node_tmp, &table->heads[i], list)
	{
		if(info_elem->orig == orig) return info_elem;
	}

	return NULL;
}

/*
 * Remove given element from the table.
 */
void
function_info_table_remove(struct function_info_table* table,
	struct function_info_elem* info_elem)
{
	hlist_del(&info_elem->list);
	function_info_elem_destroy(info_elem);
	kfree(info_elem);
}

static int function_use(struct function_info_table* table, void* orig)
{
	int result;
	struct function_info_elem* info_elem =
		function_info_table_find(table, orig);
	if(info_elem == NULL)
	{
		pr_err("Function %pf(%p) is not supported.", orig, orig);
		return -EINVAL;
	}
	result = function_info_elem_use(info_elem);
	if(result)
	{
		pr_err("Failed to fix support for function %pf(%p).", orig, orig);
		return result;
	}
	
	return 0;
}

static void function_unuse(struct function_info_table* table, void* orig)
{
	struct function_info_elem* info_elem =
		function_info_table_find(table, orig);
	BUG_ON(info_elem == NULL);
	function_info_elem_unuse(info_elem);
}

static int function_use_replace(struct function_info_table* table, void* orig)
{
	int result;
	struct function_info_elem* info_elem =
		function_info_table_find(table, orig);
	if(info_elem == NULL)
	{
		pr_err("Function %pf(%p) is not supported.", orig, orig);
		return -EINVAL;
	}
	if(info_elem->is_replaced)
	{
		pr_err("Function %pf(%p) is already replaced, cannot replace it twice.",
			orig, orig);
		return -EBUSY;
	}

	result = function_info_elem_use(info_elem);
	if(result)
	{
		pr_err("Failed to fix support for function %pf(%p).", orig, orig);
		return result;
	}
	info_elem->is_replaced = 1;
	
	return 0;
}

static void function_unuse_replace(struct function_info_table* table, void* orig)
{
	struct function_info_elem* info_elem =
		function_info_table_find(table, orig);
	BUG_ON(info_elem == NULL);
	BUG_ON(!info_elem->is_replaced);
	
	info_elem->is_replaced = 0;
	function_info_elem_unuse(info_elem);
}

int
function_info_table_add_payload(struct function_info_table* table,
	struct kedr_payload* payload)
{
	int result;

	if(payload->pre_pairs)
	{
		struct kedr_pre_pair* pre_pair;
		
		for(pre_pair = payload->pre_pairs; pre_pair->orig != NULL; pre_pair++)
		{
			result = function_use(table, pre_pair->orig);
			if(result) break;
		}
		if(pre_pair->orig != NULL)
		{
			//Error
			for(--pre_pair; (pre_pair - payload->pre_pairs) >= 0; pre_pair--)
			{
				function_unuse(table, pre_pair->orig);
			}
			goto err_pre;
		}
	}

	if(payload->post_pairs)
	{
		struct kedr_post_pair* post_pair;

		for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
		{
			result = function_use(table, post_pair->orig);
			if(result) break;
		}
		if(post_pair->orig != NULL)
		{
			//Error
			for(--post_pair; (post_pair - payload->post_pairs) >= 0; post_pair--)
			{
				function_unuse(table, post_pair->orig);
			}
			goto err_post;
		}
	}

	if(payload->replace_pairs)
	{
		struct kedr_replace_pair* replace_pair;

		for(replace_pair = payload->replace_pairs; replace_pair->orig != NULL; replace_pair++)
		{
			result = function_use_replace(table, replace_pair->orig);
			if(result) break;
		}
		if(replace_pair->orig != NULL)
		{
			//Error
			for(--replace_pair; (replace_pair - payload->replace_pairs) >= 0; replace_pair--)
			{
				function_unuse_replace(table, replace_pair->orig);
			}
			goto err_replace;
		}
	}

	return 0;
	
err_replace:
	if(payload->post_pairs)
	{
		struct kedr_post_pair* post_pair;

		for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
		{
			function_unuse(table, post_pair->orig);
		}
	}
err_post:
	if(payload->pre_pairs)
	{
		struct kedr_pre_pair* pre_pair;
		for(pre_pair = payload->pre_pairs; pre_pair->orig != NULL; pre_pair++)
		{
			function_unuse(table, pre_pair->orig);
		}
	}
err_pre:
	return result;
}
void
function_info_table_remove_payload(struct function_info_table* table,
	struct kedr_payload* payload)
{
	if(payload->replace_pairs)
	{
		struct kedr_replace_pair* replace_pair;

		for(replace_pair = payload->replace_pairs; replace_pair->orig != NULL; replace_pair++)
		{
			function_unuse_replace(table, replace_pair->orig);
		}
	}

	if(payload->post_pairs)
	{
		struct kedr_post_pair* post_pair;

		for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
		{
			function_unuse(table, post_pair->orig);
		}
	}

	if(payload->pre_pairs)
	{
		struct kedr_pre_pair* pre_pair;
		for(pre_pair = payload->pre_pairs; pre_pair->orig != NULL; pre_pair++)
		{
			function_unuse(table, pre_pair->orig);
		}
	}
}

int
function_info_table_add_support(struct function_info_table* table,
	struct functions_support_elem* support_elem)
{
	int result;
	struct kedr_intermediate_impl* impl;
	struct kedr_functions_support* functions_support = support_elem->functions_support;
	
	for(impl = functions_support->intermediate_impl;
		impl->orig != NULL;
		impl++)
	{
		struct function_info_elem* info_elem =
			function_info_table_get(table, impl->orig);
		if(IS_ERR(info_elem))
		{
			result = PTR_ERR(info_elem);
			break;
		}
		result = function_info_elem_add_support(info_elem, support_elem);
		if(result)
		{
			if(!function_info_elem_is_supported(info_elem))
				function_info_table_remove(table, info_elem);
			break;
		}
	}
	if(impl->orig == NULL) return 0;

	for(--impl;
		(impl - functions_support->intermediate_impl) >= 0;
		impl--)
	{
		struct function_info_elem* info_elem =
			function_info_table_find(table, impl->orig);
		BUG_ON(info_elem == NULL);
		function_info_elem_remove_support(info_elem, support_elem);
		if(!function_info_elem_is_supported(info_elem))
			function_info_table_remove(table, info_elem);
	}
	return result;
}

int
function_info_table_remove_support(struct function_info_table* table,
	struct functions_support_elem* support_elem)
{
	struct kedr_intermediate_impl* impl;
	struct kedr_functions_support *functions_support = support_elem->functions_support;
	int result;
	for(impl = functions_support->intermediate_impl;
		impl->orig != NULL;
		impl++)
	{
		struct function_info_elem* info_elem =
			function_info_table_find(table, impl->orig);
		BUG_ON(info_elem == NULL);
		result = function_info_elem_remove_support(info_elem, support_elem);
		if(result)
		{
			/*
			 *  Very rare situation.
			 * Even if we return error in that case,
			 * user probably will not process it correctly.
			 * So disable all useful functionality of the KEDR.
			 */
			disable_all_functionality();
			return -EINVAL;
		}
		if(!function_info_elem_is_supported(info_elem))
			function_info_table_remove(table, info_elem);
	}
	return 0;

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
	void* func)
{
	int i;
	struct hlist_node* node_tmp;
	struct function_counters_elem* elem;

	i = hash_ptr(func, table->bits);

	hlist_for_each_entry(elem, node_tmp, &table->heads[i], list)
	{
		if(elem->func == func) return elem;
	}
	
	elem = kmalloc(sizeof(*elem), GFP_KERNEL);
	if(elem == NULL)
	{
		pr_err("function_counters_table_get: Failed to allocate hash table entry.");
		return ERR_PTR(-ENOMEM);
	}
	elem->func = func;
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
intermediate_info_init(struct kedr_intermediate_info* info,
	int n_pre, int is_replaced, int n_post)
{
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

/* Add pre-function to the intermediate info */
void
intermediate_info_add_pre(struct kedr_intermediate_info* info,
	void* pre_function)
{
	void** pre_elem;
	BUG_ON(info->pre == NULL);
	for(pre_elem = info->pre; *pre_elem != NULL; pre_elem++);
	
	*pre_elem = pre_function;
	*(pre_elem + 1) = NULL;
}


/* Add post-function to the intermediate info */
void
intermediate_info_add_post(struct kedr_intermediate_info* info,
	void* post_function)
{
	void** post_elem;
	BUG_ON(info->post == NULL);
	for(post_elem = info->post; *post_elem != NULL; post_elem++);
	
	*post_elem = post_function;
	*(post_elem + 1) = NULL;
}

/* Set replace function to the intermediate info */
void
intermediate_info_set_replace(struct kedr_intermediate_info* info,
	void* replace_function)
{
	BUG_ON(info->replace != NULL);
	info->replace = replace_function;
}

void
intermediate_info_destroy(struct kedr_intermediate_info* info)
{
	kfree(info->pre);
	kfree(info->post);
}

/* Free the combined replacement table*/
void
repl_table_free(struct kedr_replace_real_pair* repl_table)
{
	struct kedr_replace_real_pair* repl_pair;
	if(repl_table == NULL) return;//nothing to do
	
	for(repl_pair = repl_table; repl_pair->orig != NULL; repl_pair++)
	{
		struct function_info_elem* info_elem;
		
		info_elem = function_info_table_find(&functions,
			repl_pair->orig);
		BUG_ON(info_elem == NULL);
		
		intermediate_info_destroy(info_elem->intermediate_info);
		function_info_elem_unuse_support(info_elem);
	}

	kfree(repl_table);

	repl_table = NULL;
}


/* Allocate appropriate amount of memory and combine the replacement 
 * tables from all fixed payload modules into a single 'table'.
 * 
 * On error return NULL.
 * 
 * Note: ERR_PTR()...
 */

struct kedr_replace_real_pair*
repl_table_create(void)
{
	struct payload_elem *elem;
	struct kedr_replace_real_pair* table = NULL;
	struct function_counters_table function_counters;
	struct function_counters_table_iter iter, iter_clear;
	int i;
	
	if(function_counters_table_init(&function_counters, 100))
	{
		return NULL;
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
				if(IS_ERR(elem)) goto err;
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
				if(IS_ERR(elem)) goto err;
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
				if(IS_ERR(elem)) goto err;
				elem->n_post++;
			}
		}
	}
	
	table = kmalloc((function_counters.n_functions + 1) * sizeof(*table), GFP_KERNEL);
	
	if(table == NULL)
	{
		pr_err("create_repl_table: Fail to allocate table.");
		goto err;
	}
	
	for(function_counters_table_begin(&function_counters, &iter), i = 0;
		iter.valid;
		function_counters_table_iter_next(&iter), i++)
	{
		int result;
		struct function_info_elem* info_elem;
		
		info_elem = function_info_table_find(&functions, iter.elem->func);
		BUG_ON(info_elem == NULL);

		/* fix intermediate implementation for given function */
		result = function_info_elem_use_support(info_elem);
		if(result)
		{
			goto err_intermediate_info;
		}

		/* allocate arrays for intermediate function */
		result = intermediate_info_init(info_elem->intermediate_info,
			iter.elem->n_pre, iter.elem->is_replaced, iter.elem->n_post);
		if(result)
		{
			function_info_elem_unuse_support(info_elem);
			goto err_intermediate_info;
		}

		BUG_ON(i >= function_counters.n_functions);
		table[i].orig = iter.elem->func;
		table[i].repl = info_elem->intermediate;
	}
	
	BUG_ON(i != function_counters.n_functions);
	table[i].orig = NULL;

	/* Fill intermediate info for every function which is used by payloads */
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
				struct function_info_elem* info_elem =
					function_info_table_find(&functions,
						pre_pair->orig);
				BUG_ON(info_elem == NULL);
				intermediate_info_add_pre(info_elem->intermediate_info,
					pre_pair->pre);
			}
		}
		
		if(payload->replace_pairs != NULL)
		{
			struct kedr_replace_pair* replace_pair;
			for(replace_pair = payload->replace_pairs; replace_pair->orig != NULL; replace_pair++)
			{
				struct function_info_elem* info_elem =
					function_info_table_find(&functions,
						replace_pair->orig);
				BUG_ON(info_elem == NULL);
				intermediate_info_set_replace(info_elem->intermediate_info,
					replace_pair->replace);
			}
		}

		if(payload->post_pairs != NULL)
		{
			struct kedr_post_pair* post_pair;
			for(post_pair = payload->post_pairs; post_pair->orig != NULL; post_pair++)
			{
				struct function_info_elem* info_elem =
					function_info_table_find(&functions,
						post_pair->orig);
				BUG_ON(info_elem == NULL);
				intermediate_info_add_post(info_elem->intermediate_info,
					post_pair->post);
			}
		}
	}
	
	function_counters_table_destroy(&function_counters);
	
	return table;

err_intermediate_info:
	/* Free all previously allocated arrays*/
	for(function_counters_table_begin(&function_counters, &iter_clear);
		iter_clear.elem != iter.elem;
		function_counters_table_iter_next(&iter_clear))
	{
		struct function_info_elem* info_elem;
		
		info_elem = function_info_table_find(&functions,
			iter_clear.elem->func);
		BUG_ON(info_elem == NULL);
		
		intermediate_info_destroy(info_elem->intermediate_info);
		
		function_info_elem_unuse_support(info_elem);
	}
	/* And free resulting table - error */
	kfree(table);
	table = NULL;

err:
	function_counters_table_destroy(&function_counters);

	return NULL;
}

/* ================================================================ */
