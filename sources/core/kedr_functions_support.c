/*
 * The "functions_support" component of KEDR system. 
 * 
 * It keeps a registry of functions support modules and provides interface 
 * for them to register / unregister themselves.
 * Allow to mark support for some function as cannot be
 * unregistered.
 * When asked, prepares and returns support for function list.
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
 
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/list.h>
#include <linux/hash.h> /* hash_ptr() */

#include <linux/mutex.h>

#include <kedr/core/kedr_functions_support.h>

#include "kedr_functions_support_internal.h"

/*
 * After some intermediate errors it is difficult to return KEDR to
 * reasonable consistent state.
 * In such cases WHOLE useful functionality of the KEDR will be disabled.
 * The only way to restore this functionality - unload KEDR module and load it again.
 * recovery
 * The same approach will be used for errors, which are recoverable for KEDR,
 * but hardly recoverable for user.
 * 
 * When this flag is set, whole functionality of this component will be disable:
 * 
 * - one cannot register any new functions_support,
 * - but one can "unregister" any functions_support
 *    (even those, which wasn't registered). Of course, this action really do nothing.
 * - kedr_functions_support_function_use will always fail,
 * - but kedr_functions_support_function_unuse will always succeed.
 * - kedr_functions_support_prepare() will always fail.
 */

static int is_disabled = 0;

/*
 * Set 'is_disable' flag and free all resources.
 * 
 * Function may be called in any internally consistent state:
 * -list of functions_support elements exists and for every element
 *  ('n_usage' != 0) is true only when element is fixed.
 * */
static void disable_all_functionality(void);


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
     * In that state adding and removing support for this function is disabled.
	 */
	int is_used_support;

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
 * Destroy function info table.
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

/* =========== Global data ========================= */

/* The list of currently loaded functions support*/
static struct list_head functions_support_list;

/*
 * Table with information about all available functions for payloads.
 */
static struct function_info_table functions;

/* 
 * Store array of replace pairs for freeing.
 * 
 * This pointer also will be used as indicator, whether
 * functions support is used now.
 */
static struct kedr_instrumentor_replace_pair* replace_pairs;

/* A mutex to protect access to the global data of kedr-functions-support*/
DEFINE_MUTEX(functions_support_mutex);

/* ================================================================ */

int kedr_functions_support_init(void)
{
	int result;
    
    INIT_LIST_HEAD(&functions_support_list);
	
	result = function_info_table_init(&functions, 100);
	if(result) return result;
    
    replace_pairs = NULL;
    
    is_disabled = 0;
    
    return 0;
}

void kedr_functions_support_destroy(void)
{
    BUG_ON(replace_pairs);
    
    if(!is_disabled)
        function_info_table_destroy(&functions);
}

/* ================================================================ */
/* Implementation of public API                                     */
/* ================================================================ */

/* Should be executed with mutex locked.*/
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
	
	result = mutex_lock_killable(&functions_support_mutex);
	if(result) return result;
	
	result = is_disabled
		? -EINVAL
        : kedr_functions_support_register_internal(functions_support);

	mutex_unlock(&functions_support_mutex);
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
		if(is_disabled) return result;//simply chain error
		pr_err("kedr_functions_support_unregister: Failed to unregister functions support.");
		return result;
	}
	
	list_del(&support_elem->list);
	kfree(support_elem);

	return 0;
}

int kedr_functions_support_unregister(struct kedr_functions_support* functions_support)
{
	int result = 0;
	
	result = mutex_lock_killable(&functions_support_mutex);
	if(result) return result;
	
	result = !is_disabled
		? kedr_functions_support_unregister_internal(functions_support)
		: 0;

	mutex_unlock(&functions_support_mutex);

	return result;
}

/* =================Interface implementation================== */

/*
 * Mark given function as used and prevent to unload support for it.
 * 
 * Several call of this function is allowed. In that case, all calls except
 * first will simply increase refcouning.
 * 
 * Return 0 on success.
 * On error return negative error code.
 */
int kedr_functions_support_function_use(void* function)
{
    int result;
    struct function_info_elem* info_elem;
    
    result = mutex_lock_killable(&functions_support_mutex);
    if(result) return result;
    
    if(is_disabled)
    {
        result = -EINVAL;
        goto out;
    }

	info_elem = function_info_table_find(&functions, function);
	if(info_elem == NULL)
	{
		pr_err("Function %pf(%p) is not supported.",
            function, function);
		result = -EINVAL;
        goto out;
	}
	result = function_info_elem_use(info_elem);
	if(result)
	{
		pr_err("Failed to fix support for function %pf(%p).",
            function, function);
		goto out;
	}

out:
    mutex_unlock(&functions_support_mutex);
    return result;
}
/*
 * Mark given function as unused and allow to unload support for it.
 * 
 * If kedr_functions_support_function_use was called more than once, than
 * this function should be called same times.
 * 
 * Return 0 on success.
 * On error return negative error code.
 */
int kedr_functions_support_function_unuse(void* function)
{
    int result;
    struct function_info_elem* info_elem;
    
    result = mutex_lock_killable(&functions_support_mutex);
    if(result) return result;

    if(is_disabled) goto out;
    
	info_elem = function_info_table_find(&functions, function);
	BUG_ON(info_elem == NULL);
	function_info_elem_unuse(info_elem);

out:
    mutex_unlock(&functions_support_mutex);
    return 0;
}

/* 
 * If return 0, then result is 'replace_pairs'.
 * Otherwise - error.
 * 
 * Should be executed with mutex locked.
 */
int
kedr_functions_support_prepare_internal(
    const struct kedr_base_interception_info* interception_info)
{
    int result;
    
    const struct kedr_base_interception_info* interception_info_elem;
    struct kedr_instrumentor_replace_pair* replace_pair;
    
    int n_elems;
    
    BUG_ON(interception_info == NULL);
    
    n_elems = 0;
    for(interception_info_elem = interception_info;
        interception_info_elem->orig != NULL;
        interception_info_elem++)
        n_elems++;
    
    replace_pairs = kmalloc(sizeof(*replace_pairs) * (n_elems + 1),
        GFP_KERNEL);
    if(replace_pairs == NULL)
    {
        pr_err("Fail to allocate replace pairs array.");
        return -ENOMEM;
    }
    
    for(interception_info_elem = interception_info, replace_pair = replace_pairs;
        interception_info_elem->orig != NULL;
        interception_info_elem++, replace_pair++)
    {
        struct function_info_elem* info_elem =
            function_info_table_find(&functions, interception_info_elem->orig);
        if(info_elem == NULL)
        {
            //Strange, but not fatal error
            pr_err("Attempt to use function %pF(%p), for which support is not registered.",
                interception_info_elem->orig, interception_info_elem->orig);
            result = -EINVAL;
            goto err_function_use;
        }
        
        result = function_info_elem_use_support(info_elem);
        if(result)
        {
            pr_err("Failed to fix support for function %pF(%p).",
                interception_info_elem->orig, interception_info_elem->orig);
            goto err_function_use;
        }
      
        replace_pair->orig = interception_info_elem->orig;
        replace_pair->repl = info_elem->intermediate;
        
        info_elem->intermediate_info->pre = interception_info_elem->pre;
        info_elem->intermediate_info->post = interception_info_elem->post;
        info_elem->intermediate_info->replace = interception_info_elem->replace;
    }
    
    replace_pair->orig = NULL;
    
    return 0;

err_function_use:

    for(--interception_info_elem;
        (interception_info_elem - interception_info) >= 0;
        interception_info_elem--)
    {
        struct function_info_elem* info_elem =
            function_info_table_find(&functions, interception_info_elem->orig);
        BUG_ON(info_elem == NULL);
        
        memset(info_elem->intermediate_info, 0, sizeof(info_elem->intermediate_info));
        function_info_elem_unuse_support(info_elem);
    }
    kfree(replace_pairs);
    replace_pairs = NULL;
    
    return result;
    
}

/*
 * Accept array of functions which should be intercepted and
 * return array of replacements for this functions, which
 * implement given interceptions.
 * 
 * After successfull call of this functions and until call of
 * kedr_function_support_release()
 * one cannot register new functions support.
 * 
 * Returning array will be freed at kedr_function_support_release() call.
 * 
 * On error return ERR_PTR().
 * */
const struct kedr_instrumentor_replace_pair*
kedr_functions_support_prepare(const struct kedr_base_interception_info* info)
{
    int result;

    result = mutex_lock_killable(&functions_support_mutex);
    if(result) return ERR_PTR(result);

    result = is_disabled
        ? -EINVAL
        : kedr_functions_support_prepare_internal(info);


    mutex_unlock(&functions_support_mutex);
    return result ? ERR_PTR(result) : replace_pairs;
}

/*
 * Release support for functions given at
 * kedr_functions_support_prepare call.
 */

void kedr_functions_support_release(void)
{
    struct kedr_instrumentor_replace_pair* replace_pair;

    /* When support is used, one cannot disable KEDR component's functionality */
    BUG_ON(is_disabled);
    
    BUG_ON(replace_pairs == NULL);

    mutex_lock(&functions_support_mutex);

    for(replace_pair = replace_pairs;
        replace_pair->orig != NULL;
        replace_pair++)
    {
        struct function_info_elem* info_elem =
            function_info_table_find(&functions, replace_pair->orig);
        BUG_ON(info_elem == NULL);
        
        memset(info_elem->intermediate_info, 0, sizeof(info_elem->intermediate_info));
        function_info_elem_unuse_support(info_elem);
    }
    
    kfree(replace_pairs);
    replace_pairs = NULL;
    
    mutex_unlock(&functions_support_mutex);
    
}


/* ======Implementation of auxiliary functions======== */

void disable_all_functionality(void)
{
	is_disabled = 1;
	function_info_table_destroy_clean(&functions);
	
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


struct functions_support_elem*
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
	/* 'info_elem' - list element for remove */
	if(info_elem->is_used_support)
	{
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
