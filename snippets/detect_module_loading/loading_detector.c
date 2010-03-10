#include <linux/module.h>

#include <linux/list.h>

MODULE_LICENSE("GPL");
// list of modules, for which detector is currently watching
struct modules_list
{
	struct list_head list;
	struct module *mod;
};
struct list_head watched_modules;

// Add module to list of watched modules
static void watched_modules_add(struct module* mod)
{
	struct modules_list *new_head = 
		kmalloc(sizeof(struct modules_list), GFP_KERNEL);
	INIT_LIST_HEAD(&new_head->list);
	new_head->mod = mod;
	list_add_tail(&new_head->list, &watched_modules);
}
// Return (not-null) pointer to node with given module, or null.
static struct modules_list* watched_modules_find(struct module* mod)
{
	struct modules_list *entry;
	struct list_head *pos;
	//look for node
	list_for_each(pos, &watched_modules)
	{
		entry = list_entry(pos, struct modules_list, list);
		if(entry->mod == mod)
			return entry;
	}
	return NULL;

}
// Remove module from list of watched modules
static void watched_modules_remove(struct module* mod)
{
	struct modules_list *head_for_delete =
		watched_modules_find(mod);
	if(head_for_delete)
	{
		//remove it
		list_del(&head_for_delete->list);
		kfree(head_for_delete);
	}
}

// Module filter.
// Should return not 0, if detector should watch for module with this name.
static int filter_module(const char *mod_name)
{
	// names of modules, for which detector should watch
	// for symplification it contains only one name
	static const char* watched_modules_names = "module_with_export";
	
	return strcmp(mod_name, watched_modules_names) == 0;
}
// There are 3 functions, which should do real work
// for interaction with modules
static void on_module_load(struct module *mod)
{
	//here should be the real work
	printk(KERN_INFO "Module '%s' has just loaded.\n",
		module_name(mod));
}

static void on_module_unload(struct module *mod)
{
	//here should be the real work
	printk(KERN_INFO "Module '%s' is going to unload.\n",
		module_name(mod));
}

static void on_detector_unload(struct module *mod)
{
	//here should be the real work
	printk(KERN_INFO "Detector stops to watch for module '%s'.\n",
		module_name(mod));
}
// Callback function for catch loading and unloading of module.
static int detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module *mod = (struct module *)vmod;

	//swith on module state
	switch(mod_state)
	{
	case MODULE_STATE_COMING:// module has just loaded
		if(!filter_module(module_name(mod))) break;
		watched_modules_add(mod);
		on_module_load(mod);
		break;
	case MODULE_STATE_GOING:// module is going to unload
		if(!watched_modules_find(mod)) break;
		on_module_unload(mod);
		watched_modules_remove(mod);
	}
	return 0;
}
// struct for watching for loading/unloading of modules.
struct notifier_block detector_nb = {
	.notifier_call = detector_notifier_call,
	.next = NULL,
	.priority = 3, /*Some number*/
};

int __init init_loading_detector(void)
{
	// initialize list of watched modules
	INIT_LIST_HEAD(&watched_modules);
	
	register_module_notifier(&detector_nb);
	printk(KERN_INFO "loading_detector module was loaded.\n");
	return 0;
	
}

void __exit exit_loading_detector(void)
{
	struct modules_list* pos, *tmp;
	unregister_module_notifier(&detector_nb);
	//destroy list of watched modules
	list_for_each_entry_safe(pos, tmp, &watched_modules, list)
	{
		on_detector_unload(pos->mod);
		list_del(&pos->list);
		kfree(pos);
	}
	printk(KERN_INFO "loading_detector module was unloaded.\n");
}

module_init(init_loading_detector);
module_exit(exit_loading_detector);
