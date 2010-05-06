/*
 * Implements API for some type of connection between kernel space
 * and user space. This connection may be used
 * (and it is developed for use) in implementing syscall-like functions
 * in user space.
 * 
 * This is the kernel part of the API.
 */

#include "syscall_connector.h"
#include "syscall_connector_internal.h"

#include <net/sock.h>
#include <net/netlink.h>

#include <linux/list.h>


/*
 * Interaction structure.
 * <in_type, pid> - unique id
 */

struct _sc_interaction
{
	__u32 pid;
	interaction_id in_type;
};

/*
 * Information about callback, registered for some type
 * of the interaction.
 * <in_type>
 */

struct type_callback_info
{
	struct list_head list;
	
	interaction_id in_type;
	
	sc_recv_callback_type cb;
	void* data;
};

/*
 * List of all callback, registered for the types.
 */

static struct list_head type_callbacks = LIST_HEAD_INIT(type_callbacks);

/*
 * Information about callback, registered for the some interaction.
 * <in_type, pid>
 */

struct callback_info
{
	struct list_head list;
	
	sc_interaction interaction;
	
	sc_recv_callback_type cb;
	void* data;
};

/*
 * List of all callback, registered for the interactions.
 */

static struct list_head callbacks = LIST_HEAD_INIT(callbacks);

// Netlink socket
struct sock *nl_sk;

// Auxiliary functions

static struct type_callback_info* 
type_callback_lookup(interaction_id in_type);
static struct callback_info*
callback_lookup(sc_interaction* interaction);
// Callback function for the socket
static void nl_data_ready(struct sk_buff* skb);

/*
 * Create interaction "channel" with user space.
 */

sc_interaction* sc_interaction_create(__u32 pid, interaction_id type)
{
	sc_interaction* interaction = kmalloc(sizeof(*interaction), GFP_KERNEL);
	if(!interaction)
	{
		printk(KERN_INFO "Cannot allocate memory for interaction descriptor.\n");
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

interaction_id sc_interaction_get_type(const sc_interaction* interaction)
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
		printk("Cannot create reply object.\n");
		return -1;
	}
	nlh = NLMSG_NEW(skb, interaction->pid,
		0/*seq*/, 0, payload_len, 0);
	
	sc_msg_put(&msg, nlmsg_data(nlh));
	
	netlink_unicast(nl_sk, skb, interaction->pid, MSG_DONTWAIT);
	return 0;
	//this mark is needed for use of NLMSG_NEW macro
nlmsg_failure:
	printk(KERN_INFO "Cannot create reply message.\n");
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

int sc_register_callback_for_type(interaction_id type,
	sc_recv_callback_type cb, void* data)
{
	struct type_callback_info* new_cbi;
	if(type_callback_lookup(type)) return 1;//already exist
	new_cbi = kmalloc(sizeof(*new_cbi), GFP_KERNEL);
	if(new_cbi == NULL)
	{
		printk(KERN_INFO "Cannot allocate memory for callback.\n");
		return -1;
	}
	new_cbi->in_type = type;
	new_cbi->cb = cb;
	new_cbi->data = data;
	
	list_add_tail(&new_cbi->list, &type_callbacks);
	return 0;
}
EXPORT_SYMBOL(sc_register_callback_for_type);

int sc_unregister_callback_for_type(interaction_id type)
{
	struct type_callback_info* cbi = type_callback_lookup(type);

	if(!cbi) return 1;
	
	list_del(&cbi->list);
	kfree(cbi);
	
	return 0;
}
EXPORT_SYMBOL(sc_unregister_callback_for_type);

/*
 * Register function, which will be called when message of given type
 * and given pid is recieved by the kernel.
 * 
 * 'msg' contains message recieved.
 * 'data' is 'cb_data'.
 */

int sc_register_callback(sc_interaction* interaction,
	sc_recv_callback_type cb, void* cb_data)
{
	struct callback_info* new_cbi;
	if(callback_lookup(interaction)) return 1;//already exist
	new_cbi = kmalloc(sizeof(*new_cbi), GFP_KERNEL);
	if(new_cbi == NULL)
	{
		printk(KERN_INFO "Cannot allocate memory for callback.\n");
		return -1;
	}
	new_cbi->interaction.in_type = interaction->in_type;
	new_cbi->interaction.pid = interaction->pid;
	new_cbi->cb = cb;
	new_cbi->data = cb_data;
	
	list_add_tail(&new_cbi->list, &callbacks);
	return 0;


}

int sc_unregister_callback(sc_interaction* interaction)
{
	struct callback_info* cbi = callback_lookup(interaction);

	if(!cbi) return 1;
	
	list_del(&cbi->list);
	kfree(cbi);

	return 0;
}

////////////////////////////////////////////////////////////////
static void nl_data_ready(struct sk_buff* skb)
{
	struct nlmsghdr	*nlh = nlmsg_hdr(skb);
	struct sc_msg msg;
	sc_interaction interaction;
	if(sc_msg_get(&msg, nlmsg_data(nlh), nlmsg_len(nlh)))
	{
		printk(KERN_INFO "Incorrect format of the message.\n");
		return;
	}
	interaction.in_type = msg.in_type;
	interaction.pid = NETLINK_CB(skb).pid;
	{
		struct callback_info* cbi = callback_lookup(&interaction);
		if(cbi != NULL)
		{
			
			cbi->cb(&interaction, msg.payload, msg.payload_length,
				cbi->data);
			wake_up_interruptible(skb->sk->sk_sleep);
			return;
		}
	}
	{
		struct type_callback_info* cbi = 
			type_callback_lookup(msg.in_type);
		if(cbi != NULL)
		{
			cbi->cb(&interaction, msg.payload, msg.payload_length,
				cbi->data);
			wake_up_interruptible(skb->sk->sk_sleep);
			return;
		}

	}
	printk("Unknown type of message.\n");
	wake_up_interruptible(skb->sk->sk_sleep);
}

static struct type_callback_info* 
type_callback_lookup(interaction_id in_type)
{
	struct type_callback_info* cbi;
	list_for_each_entry(cbi, &type_callbacks, list)
	{
		if(cbi->in_type == in_type)
			return cbi;
	}
	return NULL;
}
static struct callback_info*
callback_lookup(sc_interaction *interaction)
{
	struct callback_info* cbi;
	list_for_each_entry(cbi, &type_callbacks, list)
	{
		if(cbi->interaction.pid == interaction->pid
			&& cbi->interaction.in_type == interaction->in_type)
			return cbi;
	}
	return NULL;
}

static __init int signal_connector_init(void)
{
	nl_sk = netlink_kernel_create(&init_net,
				SC_NETLINK_PROTO,
				0,
				nl_data_ready,
				NULL,
				THIS_MODULE);
	if(nl_sk == NULL)
	{
		printk(KERN_INFO "netlink_kernel_create returns NULL.\n");
		return -1;
	}
	return 0;
}

static __exit void signal_connector_exit(void)
{
	BUG_ON(!list_empty(&callbacks));
	BUG_ON(!list_empty(&type_callbacks));
	netlink_kernel_release(nl_sk);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tsyvarev Andrey");

module_init(signal_connector_init);
module_exit(signal_connector_exit);