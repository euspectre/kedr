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

#include <net/sock.h>
#include <net/netlink.h>

#include <linux/list.h>

#include <linux/spinlock.h>

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
 */

struct type_callback_info
{
	struct list_head list;
	
	sc_interaction_id in_type;
	
	sc_recv_callback_type cb;
	void* data;
};

/*
 * List of all callback, registered for the types.
 */

static struct list_head type_callbacks = LIST_HEAD_INIT(type_callbacks);

/*
 * Protect 'type_callbacks' list from concurrent read and write.
 */

static spinlock_t type_callbacks_spinlock;

/*
 * Information about callback, registered for the some interaction channel.
 * <in_type, pid>
 */

struct channel_callback_info
{
	struct list_head list;
	
	sc_interaction interaction;
	
	sc_recv_callback_type cb;
	void* data;
};

/*
 * List of all callback, registered for the interactions channels.
 */

static struct list_head channel_callbacks = LIST_HEAD_INIT(channel_callbacks);

/*
 * Protect 'channel_callbacks' list from concurrent read and write.
 */

static spinlock_t channel_callbacks_spinlock;


// Netlink socket
struct sock *nl_sk;

// Auxiliary functions

/*
 * Get callback for given interaction type or NULL.
 * Should be executed under 'type_callbacks_spinlock' taken.
 */
 
static struct type_callback_info* 
type_callback_lookup(sc_interaction_id in_type);

/*
 * Get callback for given interaction channel or NULL.
 * Should be executed under 'channel_callbacks_spinlock' taken.
 */

static struct channel_callback_info*
channel_callback_lookup(sc_interaction* interaction);

// Callback function for the socket
static void nl_data_ready(struct sk_buff* skb);

/*
 * Create interaction channel with the user space.
 */

sc_interaction* sc_interaction_create(sc_interaction_id type, __u32 pid)
{
	sc_interaction* interaction = kmalloc(sizeof(*interaction), GFP_KERNEL);
	if(!interaction)
	{
		print_error0("Cannot allocate memory for interaction descriptor.");
		return NULL;
	}
	interaction->in_type = type;
	interaction->pid = pid;
	return interaction;
}
EXPORT_SYMBOL(sc_interaction_create);

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

void sc_interaction_destroy(sc_interaction* interaction)
{
	kfree(interaction);
}
EXPORT_SYMBOL(sc_interaction_destroy);

/*
 * Send given message via given interaction.
 */

int sc_send(sc_interaction* interaction, const void* buf, size_t len)
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
	
	netlink_unicast(nl_sk, skb, interaction->pid, MSG_DONTWAIT);
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
	sc_recv_callback_type cb, void* data)
{
	struct type_callback_info* new_tci;
    unsigned long flags;
    
	new_tci = kmalloc(sizeof(*new_tci), GFP_KERNEL);
	if(new_tci != NULL)
	{
		print_error0("Cannot allocate memory for callback.");
		return -1;
	}
	new_tci->in_type = type;
	new_tci->cb = cb;
	new_tci->data = data;


	spin_lock_irqsave(&type_callbacks_spinlock, flags);
	if(type_callback_lookup(type))
    {
        spin_unlock_irqrestore(&type_callbacks_spinlock, flags);
        print_error("Callback already exists for type %d.", type);
        kfree(new_tci);
        return 1;//already exist
    }
	list_add_tail(&new_tci->list, &type_callbacks);
    spin_unlock_irqrestore(&type_callbacks_spinlock, flags);
	return 0;
}
EXPORT_SYMBOL(sc_register_callback_for_type);

int sc_unregister_callback_for_type(sc_interaction_id type)
{
	unsigned long flags;
	struct type_callback_info* tci;
	
	spin_lock_irqsave(&type_callbacks_spinlock, flags);
	tci = type_callback_lookup(type);
	if(tci)
    {
    	list_del(&tci->list);
    	kfree(tci);
    }
    spin_unlock_irqrestore(&type_callbacks_spinlock, flags);

	return tci == NULL;
}
EXPORT_SYMBOL(sc_unregister_callback_for_type);

/*
 * Register function, which will be called when message of given type
 * and given pid is recieved by the kernel.
 * 
 * 'msg' contains message recieved.
 * 'data' is 'cb_data'.
 */

int sc_register_callback_for_channel(sc_interaction* interaction,
	sc_recv_callback_type cb, void* cb_data)
{
	unsigned long flags;
	struct channel_callback_info* new_cci;
	new_cci = kmalloc(sizeof(*new_cci), GFP_KERNEL);
	if(new_cci == NULL)
	{
		print_error0("Cannot allocate memory for callback.");
		return -1;
	}
	new_cci->interaction.in_type = interaction->in_type;
	new_cci->interaction.pid = interaction->pid;
	new_cci->cb = cb;
	new_cci->data = cb_data;

    spin_lock_irqsave(&channel_callbacks_spinlock, flags);
	if(channel_callback_lookup(interaction))
    {
        spin_unlock_irqrestore(&channel_callbacks_spinlock, flags);
        print_error("Callback already exists for channel (%d,%u).",
            sc_interaction_get_type(interaction),
            sc_interaction_get_pid(interaction)
        );
        kfree(new_cci);
        return 1;//already exist
    }
	list_add_tail(&new_cci->list, &channel_callbacks);
    spin_unlock_irqrestore(&channel_callbacks_spinlock, flags);

	return 0;
}
EXPORT_SYMBOL(sc_register_callback_for_channel);

int sc_unregister_callback_for_channel(sc_interaction* interaction)
{
	unsigned long flags;
	struct channel_callback_info* cci;

    spin_lock_irqsave(&channel_callbacks_spinlock, flags);    
    cci = channel_callback_lookup(interaction);
	if(cci)
	{
    	list_del(&cci->list);
    	kfree(cci);
    }
    spin_unlock_irqrestore(&channel_callbacks_spinlock, flags);

	return cci == NULL;
}
EXPORT_SYMBOL(sc_unregister_callback_for_channel);
////////////////////////////////////////////////////////////////
static void nl_data_ready(struct sk_buff* skb)
{
    unsigned long flags;
	struct nlmsghdr	*nlh = nlmsg_hdr(skb);
	struct sc_msg msg;
	sc_interaction interaction;

    struct channel_callback_info* cci;
    struct type_callback_info* tci;
    
    sc_recv_callback_type cb = NULL;
	void* cb_data = NULL;
    
	if(sc_msg_get(&msg, nlmsg_data(nlh), nlmsg_len(nlh)))
	{
		print_error0("Incorrect format of the message recieved.");
		return;
	}
	interaction.in_type = msg.in_type;
	interaction.pid = NETLINK_CB(skb).pid;
    // look for callback for channel
	spin_lock_irqsave(&channel_callbacks_spinlock, flags);    
	cci = channel_callback_lookup(&interaction);
    if(cci != NULL)
    {
        cb = cci->cb;
        cb_data = cci->data;
    }
    spin_unlock_irqrestore(&channel_callbacks_spinlock, flags);
    // If no callback is registered for channel, look for callback for type
    if(cci == NULL)
    {
    	spin_lock_irqsave(&type_callbacks_spinlock, flags);    
    	tci = type_callback_lookup(msg.in_type);
        if(tci != NULL)
        {
            cb = tci->cb;
            cb_data = tci->data;
        }
        spin_unlock_irqrestore(&type_callbacks_spinlock, flags);
    }
    //If callback is found, call it
	if(cb != NULL)
	{
		cb(&interaction, msg.payload, msg.payload_length,
			cb_data);
	}
    else
    {
    	print_error("No callback is registered for message, recieved from channel (%d, %u).",
            sc_interaction_get_type(&interaction),
            sc_interaction_get_pid(&interaction)
        );
    }
	wake_up_interruptible(skb->sk->sk_sleep);
}

static struct type_callback_info* 
type_callback_lookup(sc_interaction_id in_type)
{
	struct type_callback_info* tci;
	list_for_each_entry(tci, &type_callbacks, list)
	{
		if(tci->in_type == in_type)
			return tci;
	}
	return NULL;
}

static struct channel_callback_info*
channel_callback_lookup(sc_interaction *interaction)
{
	struct channel_callback_info* cci;
	list_for_each_entry(cci, &channel_callbacks, list)
	{
		if(cci->interaction.pid == interaction->pid
			&& cci->interaction.in_type == interaction->in_type)
			return cci;
	}
	return NULL;
}

static __init int syscall_connector_init(void)
{
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
    spin_lock_init(&type_callbacks_spinlock);
    spin_lock_init(&channel_callbacks_spinlock);
	return 0;
}

static __exit void syscall_connector_exit(void)
{
	BUG_ON(!list_empty(&channel_callbacks));
	BUG_ON(!list_empty(&type_callbacks));
	netlink_kernel_release(nl_sk);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tsyvarev Andrey");

module_init(syscall_connector_init);
module_exit(syscall_connector_exit);