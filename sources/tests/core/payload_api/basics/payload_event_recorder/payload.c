/*
 * Payload module which just record kedr events.
 * 
 * Set of event handlers is configured via module's parameters.
 * 
 * Each handler just record event into the list, which can be read
 * then through file.
 */
 
#include <linux/string.h>   /* kstrdup() */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/slab.h>     /* __kmalloc() */

#include <kedr/core/kedr.h>

/*********************************************************************/
MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");
/*********************************************************************/

/* Whether need to setup corresponded callback when register payload */
#define DEFINE_EVENT_HANDLER_FLAG(name) \
static unsigned int name = 0; \
module_param(name, uint, S_IRUGO)

DEFINE_EVENT_HANDLER_FLAG(set_load_fn); // on_target_loaded
DEFINE_EVENT_HANDLER_FLAG(set_unload_fn); // on_target_about_to_unload
DEFINE_EVENT_HANDLER_FLAG(set_start_fn); // on_session_start
DEFINE_EVENT_HANDLER_FLAG(set_end_fn); // on_session_end

/* List of recored events and lock protected it. */
static LIST_HEAD(events_list);
static DEFINE_SPINLOCK(events_list_lock);

/* One event record. */
struct event
{
	struct list_head elem;
	/* Null-terminated string, described event. */
	char message[1];
};

/* 
 * Create event with printf-formed message and add it to the list.
 * 
 * Return 0 on success, negative error code on fail.
 */
__printf(1,2)
static int event_add(const char* format, ...);

int event_add(const char* format, ...)
{
	unsigned long flags;
	struct event* e;
	int msg_len;
	va_list v;
	
	// Compute message length.
	va_start(v, format);
	msg_len = vsnprintf(NULL, 0, format, v);
	va_end(v);
	// Allocate event.
	e = kmalloc(offsetof(struct event, message) + msg_len + 1, GFP_KERNEL);
	if(!e) return -ENOMEM;
	
	// Actually write message.
	va_start(v, format);
	vsnprintf(e->message, msg_len + 1, format, v);
	va_end(v);
	
	// Add event into the list.
	spin_lock_irqsave(&events_list_lock, flags);
	list_add_tail(&e->elem, &events_list);
	spin_unlock_irqrestore(&events_list_lock, flags);
	
	return 0;
}

static void clear_events(void)
{
	unsigned long flags;
	struct event* e;
	
	spin_lock_irqsave(&events_list_lock, flags);
	while(!list_empty(&events_list))
	{
		e = list_first_entry(&events_list, typeof(*e), elem);
		list_del(&e->elem);
		kfree(e);
	}
	spin_unlock_irqrestore(&events_list_lock, flags);
}

/*********************************************************************
 * Replacement functions
 *********************************************************************/
static void*
repl___kmalloc(size_t size, gfp_t flags,
	struct kedr_function_call_info* call_info)
{
	return __kmalloc(size, flags);
}

/*********************************************************************
 * Callbacks for events
 *********************************************************************/
static void
on_target_loaded(struct module *target_module)
{
	char *name = NULL;
	
	BUG_ON(target_module == NULL);
	name = module_name(target_module);
	
	event_add("load(%s)", name);
}

static void
on_target_about_to_unload(struct module *target_module)
{
	char *name = NULL;
	
	BUG_ON(target_module == NULL);
	name = module_name(target_module);
	
	event_add("unload(%s)", name);
}

static void
on_session_start(void)
{
	event_add("%s", "start");
}

static void
on_session_end(void)
{
	event_add("%s", "end");
}

/*********************************************************************/
/* Sequencial file for output events. */
struct dentry* events_file;

static void* events_file_seq_start(struct seq_file* m, loff_t* pos)
{
	spin_lock_irq(&events_list_lock);
	
	return seq_list_start(&events_list, *pos);
}

static void events_file_seq_stop(struct seq_file* m, void* v)
{
	spin_unlock_irq(&events_list_lock);
}


static void* events_file_seq_next(struct seq_file* m, void* v,
	loff_t* pos)
{
	return seq_list_next(v, &events_list, pos);
}

static int events_file_seq_show(struct seq_file* m, void* v)
{
	struct event* e = list_entry(v, typeof(*e), elem);
	if(e->elem.prev != &events_list)
		seq_putc(m, ',');

	seq_printf(m, "%s", e->message);
	
	return 0;
}

static struct seq_operations events_file_seq_ops =
{
	.start = events_file_seq_start,
	.stop = events_file_seq_stop,
	.next = events_file_seq_next,
	.show = events_file_seq_show
};


static int events_file_open(struct inode* inode, struct file* file)
{
	return seq_open(file, &events_file_seq_ops);
}

static struct file_operations events_file_ops =
{
	.owner = THIS_MODULE,
	
	.open = events_file_open,
	.release = seq_release,
	
	.read = seq_read
};

/*********************************************************************/

static struct kedr_replace_pair replace_pairs[] =
{
	{
		.orig = (void*)&__kmalloc,
		.replace = (void*)&repl___kmalloc
	},
	{
		.orig = NULL
	}
};

static struct kedr_payload payload = {
	.mod            = THIS_MODULE,
	.replace_pairs	= replace_pairs
};

/*********************************************************************/

extern int functions_support_register(void);
extern void functions_support_unregister(void);

static void
kedr_test_cleanup_module(void)
{
	kedr_payload_unregister(&payload);
	functions_support_unregister();
	debugfs_remove(events_file);
	clear_events();
}

static int __init
kedr_test_init_module(void)
{
	int result = 0;
	
	events_file = debugfs_create_file("event_recorder", S_IRUGO,
		NULL, NULL, &events_file_ops);
		
	if(!events_file) return -ENOMEM;

	result = functions_support_register();
	if(result) goto err_functions;
	
	if (set_load_fn != 0)
		payload.on_target_loaded = on_target_loaded;
	
	if (set_unload_fn != 0)
		payload.on_target_about_to_unload = on_target_about_to_unload;
	
	if (set_start_fn != 0)
		payload.on_session_start = on_session_start;
	
	if (set_end_fn != 0)
		payload.on_session_end = on_session_end;


	result = kedr_payload_register(&payload);
	if (result != 0)
		goto err_payload;
	
	return 0;

err_payload:
	functions_support_unregister();
err_functions:
	debugfs_remove(events_file);
	return result;
}

module_init(kedr_test_init_module);
module_exit(kedr_test_cleanup_module);
/*********************************************************************/
