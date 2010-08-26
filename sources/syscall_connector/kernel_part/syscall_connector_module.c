/*
 * Implements API for some type of connection between kernel space
 * and user space. This connection may be used
 * (and it is developed for use) in implementing syscall-like functions
 * in user space.
 * 
 * This is the kernel part of the API.
 */

#include <kedr/syscall_connector/syscall_connector.h>
#include <kedr/syscall_connector/syscall_connector_internal.h>
#include <kedr/syscall_connector/uobject.h>

#include <net/sock.h>
#include <net/netlink.h>

#include <linux/list.h>

#include <linux/spinlock.h>

#include <linux/wait.h>

#define debug(str, ...) printk(KERN_DEBUG "%s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printk(KERN_ERR "%s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

/*
 * Interaction structure.
 * <in_type, pid> - unique id of interaction channel
 */

struct _sc_interaction
{
	sc_interaction_id in_type;
	__u32 pid;
};


/*
 * Information about callback, registered for some type
 * of the interaction.
 * <in_type>
 *
 * Every node of callback for type also contains list of callbacks
 * for channels of interactions of given type.
 */

struct type_callback_info
{
	struct list_head list;
	
	sc_interaction_id in_type;
	
	sc_recv_callback_t cb;
	void* data;
    void (*destroy) (void*);
    // list of channel callbacks for interactions of given type
    struct list_head channel_callbacks;
    // whether someone wait to delete this entry
    int is_waiting_deleting;
    // waitqueue for wait until object may be destroyed.
    wait_queue_head_t wait_finish;
    struct uobject obj;
};
//hint for search unused type, currently it is always 0
sc_interaction_id in_type_hint = 0;

/*
 * Information about callback, registered for some interaction channel.
 * <in_type, pid>
 *
 * Interaction type(in_type) will be stored not here.
 */

struct channel_callback_info
{
	struct list_head list;
	
	__u32 pid;
	
	sc_recv_callback_t cb;
	void* data;
    void (*destroy) (void*);
    //'parent' callback info
    struct type_callback_info* tci;
    struct uobject obj;
};

/*
 * List of all callback, registered for the types.
 */

static struct list_head callbacks = LIST_HEAD_INIT(callbacks);

/*
 * Protect 'callbacks' list and sublists from concurrent read and write.
 */

static spinlock_t callbacks_spinlock;

// Netlink socket
struct sock *nl_sk;

///////////////////Auxiliary functions//////////////////////////

/*
 * Initialise structure type_callback_info.
 */

static void type_callback_info_init(struct type_callback_info* tci,
    sc_recv_callback_t cb,
    void* data, void (*destroy) (void*)
    );

// Destroy structure type_callback_info.
// May be used in atomic context.
static void type_callback_info_destroy(struct type_callback_info* tci);

/*
 * Initialise structure channel_callback_info.
 */

static void channel_callback_info_init(struct channel_callback_info* cci,
    __u32 pid, sc_recv_callback_t cb,
    void* data, void (*destroy) (void*)
    );

    
// Destroy structure channel_callback_info.
// May be used in atomic context.
static void channel_callback_info_destroy(struct channel_callback_info* cci);

//Notifiers for uobject
static void type_callback_invalidate_notifier(struct uobject* obj);
static void type_callback_finalize_notifier(struct uobject* obj);
static void channel_callback_invalidate_notifier(struct uobject* obj);
static void channel_callback_finalize_notifier(struct uobject* obj);

//Destroy struct type_callback_info, for reuse in finalize notifier and in waiting deleting
static void finalize_callback_for_type(struct type_callback_info* tci)
{
    type_callback_info_destroy(tci);
    kfree(tci);
}

/*
 * Get callback for given interaction type or NULL.
 * Should be executed under 'callbacks_spinlock' taken.
 *
 * If may_deleting is not 0, also find nodes for which is_deleting flag is set.
 */
 
static struct type_callback_info* 
type_callback_lookup(sc_interaction_id in_type);

/*
 * Get callback for given interaction channel or NULL.
 * Should be executed under 'callbacks_spinlock' taken.
 */

static struct channel_callback_info*
channel_callback_lookup(struct type_callback_info*, __u32 pid);

/*
 * Insert record for callback for type into callbacks list.
 *
 * Internal function, should be executed under lock taken.
 */
static void add_callback_for_type_internal(struct type_callback_info*, sc_interaction_id in_type);

// Callback function for the socket
static void nl_data_ready(struct sk_buff* skb);

/*
 * Implementation of named libraries.
 */

struct named_library
{
    struct list_head list;

    const char* name;
    
    int (*try_use)(void);
    void (*unuse)(void);
    
    const void* reply_msg;
    size_t reply_msg_len;
    // waitqueue for wait until object may be destroyed.
    wait_queue_head_t wait_finish;
    //
    struct uobject obj;
};

//static struct list_head callbacks = LIST_HEAD_INIT(callbacks);
static struct list_head named_libraries = LIST_HEAD_INIT(named_libraries);//list of libraries
static spinlock_t named_libraries_spinlock;

static void named_library_init(struct named_library* library,
    int (*try_use)(void), void (*unuse)(void),
    void* reply_msg, size_t reply_msg_len
);
static void named_library_destroy(struct named_library* library);
/*
 * Add library record into the list of named libraries
 *
 * Internal function, should be used under lock taken.
 */
static void add_library_internal(struct named_library* library, const char* library_name);
//search library with given name, should be executed under spinlock taken
static struct named_library* named_libraries_look_for(const char* name);

static void named_library_invalidate_notifier(struct uobject* obj);
static void named_library_finalize_notifier(struct uobject* obj);
/////////////////////////////////////////////////////////////////////////////////
__u32 sc_interaction_get_pid(const sc_interaction* interaction)
{
	return interaction->pid;
}
EXPORT_SYMBOL(sc_interaction_get_pid);

sc_interaction_id sc_interaction_get_type(const sc_interaction* interaction)
{
	return interaction->in_type;
}
EXPORT_SYMBOL(sc_interaction_get_type);

/*
 * Send given message via given interaction.
 */

int sc_send(const sc_interaction* interaction, const void* buf, size_t len)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct sc_msg msg = {
		.in_type = interaction->in_type,
		.payload = buf,
		.payload_length = len
	};
	
	size_t payload_len = sc_msg_len(&msg);
	
	skb = nlmsg_new(payload_len, GFP_KERNEL);
	if(!skb)
	{
		print_error0("Cannot create object for send message.");
		return -1;
	}
	nlh = NLMSG_NEW(skb, interaction->pid,
		0/*seq*/, 0, payload_len, 0);
	
	sc_msg_put(&msg, nlmsg_data(nlh));
	
	if(!netlink_unicast(nl_sk, skb, interaction->pid, 0))
        return -1;
	return 0;
	//this mark is needed for use of NLMSG_NEW macro
nlmsg_failure:
	print_error0("Cannot create message for send.");
	kfree_skb(skb);
	return -1;
}
EXPORT_SYMBOL(sc_send);

/*
 * Register function, which will be called when message of given type
 * is recieved by the kernel.
 * 
 * 'msg' contains message recieved.
 * 'data' is NULL.
 */

int sc_register_callback_for_type(sc_interaction_id type,
	sc_recv_callback_t cb, void* data, void (*destroy) (void*))
{
	struct type_callback_info* new_tci;
    unsigned long flags;
    
	new_tci = kmalloc(sizeof(*new_tci), GFP_KERNEL);
	if(new_tci == NULL)
	{
		print_error0("Cannot allocate memory for callback.");
		return -1;
	}
    type_callback_info_init(new_tci, cb, data, destroy);

	spin_lock_irqsave(&callbacks_spinlock, flags);
	if(type_callback_lookup(type))
    {
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        print_error("Callback already exists for type %d.", type);
        type_callback_info_destroy(new_tci);
        kfree(new_tci);
        return 1;//already exist
    }
    add_callback_for_type_internal(new_tci, type);

    spin_unlock_irqrestore(&callbacks_spinlock, flags);
	return 0;
}
EXPORT_SYMBOL(sc_register_callback_for_type);

sc_interaction_id sc_register_callback_for_unused_type(
	sc_recv_callback_t cb, void* data, void (*destroy)(void*))
{
	struct type_callback_info* new_tci;
    unsigned long flags;
    
    sc_interaction_id type;
    
	new_tci = kmalloc(sizeof(*new_tci), GFP_KERNEL);
	if(new_tci == NULL)
	{
		print_error0("Cannot allocate memory for callback.");
		return -1;
	}
    type_callback_info_init(new_tci, cb, data, destroy);

	spin_lock_irqsave(&callbacks_spinlock, flags);
    //skip used types
    for(type = in_type_hint; type_callback_lookup(type); type++);

    add_callback_for_type_internal(new_tci, type);
    
    spin_unlock_irqrestore(&callbacks_spinlock, flags);
    return type;
}
EXPORT_SYMBOL(sc_register_callback_for_unused_type);

int sc_unregister_callback_for_type(sc_interaction_id type, int need_wait)
{
	unsigned long flags;
	struct type_callback_info* tci;
    int result = 0;

	spin_lock_irqsave(&callbacks_spinlock, flags);
	tci = type_callback_lookup(type);
	if(!tci)
    {
        //callback doesn't exist
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        print_error("No callback is registered for type %ld.", (long)type);
        result = 1;
    }
    else if(uobject_try_use(&tci->obj))
    {
        //callback is currently deleting
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        print_error("Callback registered for type %ld is already being unregistered.", (long)type);
        result = 1;
    }
    else
    {
        uobject_invalidate(&tci->obj);//will not block, because we call try_use() previously
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
    }

    if(result)
        return 1;
    
    if(!need_wait)
    {
        uobject_unuse(&tci->obj);
        return 0;
    }
    else
    {
        DEFINE_WAIT(wait);

        tci->is_waiting_deleting = 1;
        prepare_to_wait(&tci->wait_finish, &wait, TASK_INTERRUPTIBLE);
        uobject_unuse(&tci->obj);//only after adding to the waitqueue
        schedule();
        finalize_callback_for_type(tci);
        return 0;
    }


}
EXPORT_SYMBOL(sc_unregister_callback_for_type);

/*
 * Register function, which will be called when message of given type
 * and given pid is recieved by the kernel.
 * 
 * 'msg' contains message recieved.
 */

int sc_register_callback_for_channel(const sc_interaction* interaction,
	sc_recv_callback_t cb, void* data, void (*destroy)(void*) )
{
	unsigned long flags;
    struct type_callback_info* tci;
	struct channel_callback_info* new_cci;
	new_cci = kmalloc(sizeof(*new_cci), GFP_KERNEL);
	if(new_cci == NULL)
	{
		print_error0("Cannot allocate memory for callback.");
		return -1;
	}
    channel_callback_info_init(new_cci, interaction->pid, cb, data, destroy);

    spin_lock_irqsave(&callbacks_spinlock, flags);
	tci = type_callback_lookup(interaction->in_type);
    if(!tci)
    {
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        print_error("Cannot register callback for channel (%d,%u), "
            "because callback for type %d is not registered.",
            interaction->in_type,
            interaction->pid,
            interaction->in_type);
        channel_callback_info_destroy(new_cci);
        kfree(new_cci);
        return 1;//callback for type is unregistered
    }
	if(channel_callback_lookup(tci, interaction->pid))
    {
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        print_error("Callback already exists for channel (%d,%u).",
            sc_interaction_get_type(interaction),
            sc_interaction_get_pid(interaction)
        );
        channel_callback_info_destroy(new_cci);
        kfree(new_cci);
        return 1;//already exist
    }
    /*if(tci->is_deleting)
    {
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        channel_callback_info_destroy(new_cci);
        kfree(new_cci);
        if(destroy)
            destroy(data);
        return 0;//as if callback was added, but after call of this function is removed
    }*/
    new_cci->tci = tci;
    uobject_ref(&tci->obj);
    
    uobject_init(&new_cci->obj);
    uobject_set_invalidate_notifier(&new_cci->obj, channel_callback_invalidate_notifier);
    uobject_set_finalize_notifier(&new_cci->obj, channel_callback_finalize_notifier);
	
	list_add_tail(&new_cci->list, &tci->channel_callbacks);
    spin_unlock_irqrestore(&callbacks_spinlock, flags);

	return 0;
}
EXPORT_SYMBOL(sc_register_callback_for_channel);

int sc_unregister_callback_for_channel(const sc_interaction* interaction)
{
	unsigned long flags;
	struct type_callback_info* tci;
	struct channel_callback_info* cci = NULL;

    spin_lock_irqsave(&callbacks_spinlock, flags);
    //also look up callbacks for type, which is currently being deleted
    tci = type_callback_lookup(interaction->in_type);
    if(!tci)
    {
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        return 1;//callback doesn't exist
    }
    cci = channel_callback_lookup(tci, interaction->pid);
	if(!cci)
    {
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        return 1;//callback doesn't exist
    }
    /*if(tci->is_deleting)
    {
        spin_unlock_irqrestore(&channel_callbacks_spinlock, flags);
        return 0;//as if callback was really deleted
    }*/
    list_del(&cci->list);
    spin_unlock_irqrestore(&callbacks_spinlock, flags);
    uobject_unref(&cci->obj);
	return 0;
}
EXPORT_SYMBOL(sc_unregister_callback_for_channel);
////////////////////////////////////////////////////////////////
static void nl_data_ready(struct sk_buff* skb)
{
    unsigned long flags;
	struct sc_msg msg;
	sc_interaction interaction;
    
    int result = 0;
    struct nlmsghdr *nlh = nlmsg_hdr(skb);

    struct channel_callback_info* cci = NULL;
    struct type_callback_info* tci = NULL;
    
    sc_recv_callback_t cb = NULL;
	void* cb_data = NULL;
    
	if(nlh->nlmsg_type != SC_NLMSG_TYPE)
    {
		print_error0("Incorrect type of the message recieved.");
		result = 1;
    }
	else if(sc_msg_get(&msg, nlmsg_data(nlh), nlmsg_len(nlh)))
	{
		print_error0("Incorrect format of the message recieved.");
        result = 1;
    }
    else
    {
    	interaction.in_type = msg.in_type;
    	interaction.pid = NETLINK_CB(skb).pid;
        debug("Message for interaction (%ld, %u) is recieved.",
            (long)interaction.in_type, (unsigned)interaction.pid);

        // look for callback for given interaction
    	spin_lock_irqsave(&callbacks_spinlock, flags);
        tci = type_callback_lookup(interaction.in_type);
        if(tci)
        {
            if(!uobject_try_use(&tci->obj))
            {
                cci = channel_callback_lookup(tci, interaction.pid);
                if(cci)
                {
                    uobject_try_use(&cci->obj);//always succeed

                    cb = cci->cb;
                    cb_data = cci->data;
                }
                else
                {
                    cb = tci->cb;
                    cb_data = tci->data;
                }
            }
            else
                result = 1;//callback is currently deleting
        }
        else
            result = 1;//callback is not exist
        spin_unlock_irqrestore(&callbacks_spinlock, flags);
        if(result)
        {
        	if(!tci)
            {
            	print_error("No callback is registered for message, recieved from channel (%d, %u).",
                    sc_interaction_get_type(&interaction),
                    sc_interaction_get_pid(&interaction)
                );
            }
            else
            {
                print_error("Callback, registered for message, recieved from channel (%d, %u), is currently unregistered.",
                    sc_interaction_get_type(&interaction),
                    sc_interaction_get_pid(&interaction)
                );
            }
        }
    }
    if(result)
    {
        //error occures
        return;
    }

    //Callback is found, call it
	cb(&interaction, msg.payload, msg.payload_length,
		cb_data);

    if(cci)
        uobject_unuse(&cci->obj);

    uobject_unuse(&tci->obj);

	wake_up_interruptible(skb->sk->sk_sleep);
}

/*
 * Initialise structure type_callback_info.
 */

//Notifiers
static void type_callback_invalidate_notifier(struct uobject* obj)
{
    unsigned long flags;
    
    struct channel_callback_info* cci, *cci_tmp;
    struct type_callback_info* tci = container_of(obj, struct type_callback_info, obj);
    
    if(tci->destroy)
        tci->destroy(tci->data);
    
    tci->data = NULL;
    
    //we are the only who has access to the list of callbacks for channels,
    //so we can iterate it without lock
    debug("Callback for type %ld is invalidate.", (long)tci->in_type);
    list_for_each_entry_safe(cci, cci_tmp, &tci->channel_callbacks, list)
	{
		list_del(&cci->list);
        uobject_unref(&cci->obj);
	}
    //need to unref object itself
    spin_lock_irqsave(&callbacks_spinlock, flags);
    list_del(&tci->list);
    spin_unlock_irqrestore(&callbacks_spinlock, flags);
    uobject_unref(&tci->obj);
}

static void type_callback_finalize_notifier(struct uobject* obj)
{
    struct type_callback_info* tci = container_of(obj, struct type_callback_info, obj);

    if(tci->is_waiting_deleting)
    {
        wake_up_interruptible(&tci->wait_finish);
    }
    else
    {
        finalize_callback_for_type(tci);
    }
}


static void type_callback_info_init(struct type_callback_info* tci,
    sc_recv_callback_t cb,
    void* data, void (*destroy) (void*)
    )
{
    INIT_LIST_HEAD(&tci->list);
	tci->cb = cb;
	tci->data = data;
    tci->destroy = destroy;

    INIT_LIST_HEAD(&tci->channel_callbacks);
    
    tci->is_waiting_deleting = 0;
    init_waitqueue_head(&tci->wait_finish);
}

// Destroy structure type_callback_info.
void type_callback_info_destroy(struct type_callback_info* tci)
{
}

/*
 * Initialise structure channel_callback_info.
 */

//Notifiers
static void channel_callback_invalidate_notifier(struct uobject* obj)
{
    struct channel_callback_info* cci = container_of(obj, struct channel_callback_info, obj);
    
    if(cci->destroy)
        cci->destroy(cci->data);

    cci->data = NULL;
    //no one may access to the invalid callback, so do it without lock
    if(cci->tci)
    {
        cci->tci = NULL;
        uobject_unref(&cci->tci->obj);
    }
}

static void channel_callback_finalize_notifier(struct uobject* obj)
{
    struct channel_callback_info* cci = container_of(obj, struct channel_callback_info, obj);

    channel_callback_info_destroy(cci);
    kfree(cci);
}

void channel_callback_info_init(struct channel_callback_info* cci,
    __u32 pid, sc_recv_callback_t cb,
    void* data, void (*destroy) (void*)
    )
{
	INIT_LIST_HEAD(&cci->list);
	cci->pid = pid;
    cci->cb = cb;
	cci->data = data;
    cci->destroy = destroy;
}

// Destroy structure channel_callback_info.
void channel_callback_info_destroy(struct channel_callback_info* cci)
{
}


static struct type_callback_info* 
type_callback_lookup(sc_interaction_id in_type)
{
	struct type_callback_info* tci;
	list_for_each_entry(tci, &callbacks, list)
	{
		if(tci->in_type == in_type)
			return tci;
	}
	return NULL;
}

static struct channel_callback_info*
channel_callback_lookup(struct type_callback_info* tci, __u32 pid)
{
	struct channel_callback_info* cci;
	list_for_each_entry(cci, &tci->channel_callbacks, list)
	{
		if(cci->pid == pid)
			return cci;
	}
	return NULL;
}

static void add_callback_for_type_internal(struct type_callback_info* tci, sc_interaction_id in_type)
{
    uobject_init(&tci->obj);
    uobject_set_finalize_notifier(&tci->obj, type_callback_finalize_notifier/*destroy node or wake_up process, which wait for this*/);
    uobject_set_invalidate_notifier(&tci->obj, type_callback_invalidate_notifier/*destroy(data), remove from list*/);

	tci->in_type = in_type;
	list_add_tail(&tci->list, &callbacks);
}
/*
 * Implementations of concrete syscalls.
 */

static void global_usage_service(const sc_interaction* interaction,
	const void* buf, size_t len, void* data)
{
    (void)data;//unused
    if(memcmp(buf, GLOBAL_USAGE_SERVICE_MSG_USE, len) == 0)
    {
        if(try_module_get(THIS_MODULE))
        {
            sc_send(interaction, GLOBAL_USAGE_SERVICE_MSG_REPLY, sizeof(GLOBAL_USAGE_SERVICE_MSG_REPLY));
        }
    }
    else if(memcmp(buf, GLOBAL_USAGE_SERVICE_MSG_UNUSE, len) == 0)
    {
        module_put(THIS_MODULE);
    }
    //otherwise message is silently ignored
}
//
int 
sc_library_register(const char* library_name,
    int (*try_use)(void), void (*unuse)(void),
    void* reply_msg, size_t reply_msg_len
)
{
    unsigned long flags;

    struct named_library* new_library = kmalloc(sizeof(*new_library), GFP_KERNEL);

    if(new_library == NULL)
    {
        print_error0("Cannot allocate memory.");
        return 1;
    }
    named_library_init(new_library, try_use, unuse, 
        reply_msg, reply_msg_len);

    spin_lock_irqsave(&named_libraries_spinlock, flags);
    if(named_libraries_look_for(library_name))
    {
        spin_unlock_irqrestore(&named_libraries_spinlock, flags);
        print_error("Library with name '%s' is already registered.", library_name);
        named_library_destroy(new_library);
        kfree(new_library);
        return 1;
    }
    
    add_library_internal(new_library, library_name);
    
    spin_unlock_irqrestore(&named_libraries_spinlock, flags);
    debug("Library with name '%s' was registered.", library_name);
    return 0;
}
EXPORT_SYMBOL(sc_library_register);

void sc_library_unregister(const char* library_name)
{
    unsigned long flags;
    
    struct named_library* library;

    spin_lock_irqsave(&named_libraries_spinlock, flags);
    library = named_libraries_look_for(library_name);
    if(library == NULL)
    {
        spin_unlock_irqrestore(&named_libraries_spinlock, flags);
        print_error("Library with name '%s' is not registered.", library_name);
        return;
    }
    if(uobject_try_use(&library->obj))
    {
        //library is currently being deleted
        spin_unlock_irqrestore(&named_libraries_spinlock, flags);
        print_error("Library '%s' is already being deleted now.", library_name);
        return;
    }

    uobject_invalidate(&library->obj);
    spin_unlock_irqrestore(&named_libraries_spinlock, flags);
    
    {
        DEFINE_WAIT(wait);
        prepare_to_wait(&library->wait_finish, &wait, TASK_INTERRUPTIBLE);
        uobject_unuse(&library->obj);//only after adding to the waitqueue
        schedule();
        finish_wait(&library->wait_finish, &wait);
        named_library_destroy(library);
        kfree(library);
        debug("Library with name '%s' was unregistered.", library_name);
        return;
    }
}
EXPORT_SYMBOL(sc_library_unregister);

void named_library_invalidate_notifier(struct uobject* obj)
{
    unsigned long flags;
    
    struct named_library* library = container_of(obj, struct named_library, obj);
    
    //need to unref object itself
    spin_lock_irqsave(&named_libraries_spinlock, flags);
    list_del(&library->list);
    spin_unlock_irqrestore(&named_libraries_spinlock, flags);
    uobject_unref(&library->obj);    
}

void named_library_finalize_notifier(struct uobject* obj)
{
    struct named_library* library = container_of(obj, struct named_library, obj);
    
    wake_up(&library->wait_finish);
}

void named_library_init(struct named_library* library,
    int (*try_use)(void), void (*unuse)(void),
    void* reply_msg, size_t reply_msg_len
)
{
    library->try_use = try_use;
    library->unuse = unuse;
    library->reply_msg = reply_msg;
    library->reply_msg_len = reply_msg_len;

    init_waitqueue_head(&library->wait_finish);
}

void named_library_destroy(struct named_library* library)
{

}

static void add_library_internal(struct named_library* library, const char* library_name)
{
    uobject_init(&library->obj);
    uobject_set_invalidate_notifier(&library->obj, named_library_invalidate_notifier);
    uobject_set_finalize_notifier(&library->obj, named_library_finalize_notifier);

    library->name = library_name;//expected, that library name is static string
    list_add_tail(&library->list, &named_libraries);
}

struct named_library* named_libraries_look_for(const char* name)
{
    struct named_library* library;

    list_for_each_entry(library, &named_libraries, list)
    {
        if(strcmp(library->name, name) == 0)
            return library;
    }

    return NULL;
}

static void named_libraries_service(const sc_interaction* interaction,
	const void* buf, size_t len, void* data)
{
    unsigned long flags;
    
    int result = 0;
    
    struct sc_named_libraries_send_msg send_msg;
    struct named_library* library;
    
    (void)data;//unused
    
    if(sc_named_libraries_send_msg_get(&send_msg, buf, len))
    {
        print_error0("Incorrect message format.");
        return;
    }
    if(send_msg.control == 1)
    {
        spin_lock_irqsave(&named_libraries_spinlock, flags);
        library = named_libraries_look_for(send_msg.library_name);
        if(!library)
        {
            //library name doesn't registered
            print_error("Library '%s' is not registered.", send_msg.library_name);
            result = 1;
        }
        else if(uobject_try_use(&library->obj))
        {
            //library is currently being deleted
            print_error("Library '%s' is currently being deleted.", send_msg.library_name);
            result = 1;
        }
        spin_unlock_irqrestore(&named_libraries_spinlock, flags);
        if(result)
            return;
        if(library->try_use())
        {
            print_error("Fail to use library '%s'.", send_msg.library_name);
            result = 1;
            uobject_unuse(&library->obj);
        }
        if(!result)
        {
            sc_send(interaction, library->reply_msg, library->reply_msg_len);
            debug("Library '%s' is used now.", send_msg.library_name);
        }
    }
    else if(send_msg.control == 0)
    {
        spin_lock_irqsave(&named_libraries_spinlock, flags);
        library = named_libraries_look_for(send_msg.library_name);
        if(!library)
        {
            print_error("Library '%s' is not registered.", send_msg.library_name);
            result = 1;//library name doesn't registered
        }
        spin_unlock_irqrestore(&named_libraries_spinlock, flags);
        if(result) return;
        library->unuse();
        uobject_unuse(&library->obj);
        debug("Library '%s' is unused now.", send_msg.library_name);
    }
}

static __init int syscall_connector_init(void)
{
    spin_lock_init(&callbacks_spinlock);
    spin_lock_init(&named_libraries_spinlock);
    //
	nl_sk = netlink_kernel_create(&init_net,
				SC_NETLINK_PROTO,
				0,
				nl_data_ready,
				NULL,
				THIS_MODULE);
	if(nl_sk == NULL)
	{
		print_error0("netlink_kernel_create returns NULL.");
		return -1;
	}
    
    if(sc_register_callback_for_type(GLOBAL_USAGE_SERVICE_IT, global_usage_service, NULL, NULL))
    {
        netlink_kernel_release(nl_sk);
        print_error0("Failed to register global usage service.");
        return -1;
    }
    debug("Global usage service is registered at %ld.", (long)GLOBAL_USAGE_SERVICE_IT);
    if(sc_register_callback_for_type(NAMED_LIBRARIES_SERVICE_IT, named_libraries_service, NULL, NULL))
    {
        sc_unregister_callback_for_type(GLOBAL_USAGE_SERVICE_IT, 1);
        netlink_kernel_release(nl_sk);
        print_error0("Failed to register named libraries service.");
        return -1;
    }
    debug("Named libraries service is registered at %ld.", (long)NAMED_LIBRARIES_SERVICE_IT);

	return 0;
}

static __exit void syscall_connector_exit(void)
{
	sc_unregister_callback_for_type(GLOBAL_USAGE_SERVICE_IT, 1);
    sc_unregister_callback_for_type(NAMED_LIBRARIES_SERVICE_IT, 1);
    
	BUG_ON(!list_empty(&callbacks));
	netlink_kernel_release(nl_sk);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tsyvarev Andrey");

module_init(syscall_connector_init);
module_exit(syscall_connector_exit);