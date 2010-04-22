#include "module_weak_ref.h"

#include <linux/list.h>

struct destroy_data
{
	struct list_head list;
	destroy_notify destroy;
	void* user_data;
};

struct module_weak_ref_data
{
	struct list_head list;
	struct module* m;
	struct list_head destroy_data;
};

LIST_HEAD(module_weak_ref_list);
// Auxiliary functions
static struct module_weak_ref_data* get_module_node(struct module* m);

// Called when some module is unloading.
static int 
detector_modules_unload(struct notifier_block *nb,
	unsigned long mod_state, void *vmod);

// struct for watching for loading/unloading of modules.
struct notifier_block detector_nb = {
	.notifier_call = detector_modules_unload,
	.next = NULL,
	.priority = 3, /*Some number*/
};


// Should be called for use weak reference functionality on module.
int module_weak_ref_init(void)
{
	return register_module_notifier(&detector_nb);
}
// Should be called when the functionality will no longer be used.
void module_weak_ref_destroy(void)
{
	BUG_ON(!list_empty(&module_weak_ref_list));
	unregister_module_notifier(&detector_nb);
}
// Shedule 'destroy' function to call when module is unloaded.
void module_weak_ref(struct module* m,
	destroy_notify destroy, void* user_data)
{
	struct destroy_data* ddata;
	struct module_weak_ref_data* mdata = get_module_node(m);
	if(mdata == NULL)
	{
		mdata = kmalloc(sizeof(*mdata), GFP_KERNEL);
		mdata->m = m;
		INIT_LIST_HEAD(&mdata->destroy_data);
		list_add_tail(&mdata->list, &module_weak_ref_list);
	}
	ddata = kmalloc(sizeof(*ddata), GFP_KERNEL);
	ddata->destroy = destroy;
	ddata->user_data = user_data;
	list_add_tail(&ddata->list, &mdata->destroy_data);
}
// Cancel sheduling.
void module_weak_unref(struct module* m,
	destroy_notify destroy, void* user_data)
{
	struct destroy_data* ddata;
	struct module_weak_ref_data* mdata = get_module_node(m);
	
	BUG_ON(mdata == NULL);
	list_for_each_entry(ddata, &mdata->destroy_data, list)
	{
		if(ddata->destroy == destroy && ddata->user_data == user_data)
		{
			list_del(&ddata->list);
			kfree(ddata);
			break;
		}
	}
	if(list_empty(&mdata->destroy_data))
	{
		list_del(&mdata->list);
		kfree(mdata);
	}
}

static struct module_weak_ref_data* get_module_node(struct module* m)
{
	struct module_weak_ref_data* node;
	list_for_each_entry(node, &module_weak_ref_list, list)
	{
		if(node->m == m) return node;
	}
	return NULL;
}

static int 
detector_modules_unload(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module_weak_ref_data* mdata;
	struct destroy_data* ddata, *tmp;
	//If module is not unloading do nothing
	if(mod_state != MODULE_STATE_GOING) return 0;
	mdata = get_module_node(vmod);
	if(mdata == NULL) return 0;
	
	list_for_each_entry_safe(ddata, tmp, &mdata->destroy_data, list)
	{
		ddata->destroy(vmod, ddata->user_data);
		list_del(&ddata->list);
		kfree(ddata);
	}
	list_del(&mdata->list);
	kfree(mdata);
	return 0;
}
