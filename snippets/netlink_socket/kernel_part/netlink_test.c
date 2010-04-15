#include <linux/init.h>
#include <linux/module.h>

#include <net/sock.h>
#include <net/netlink.h>
#include <linux/kernel.h>

MODULE_LICENSE("Dual BSD/GPL");

#define NETLINK_TEST 17

struct sock *nl_sk = NULL;
// Replies to the sender of skb with the same message.
static void my_echo(struct sk_buff *skb)
{
	struct sk_buff *skb_rep;
	struct nlmsghdr *nlh_rep;
	
	struct nlmsghdr	*nlh = nlmsg_hdr(skb);
	size_t payload = nlmsg_len(nlh);
	
	skb_rep = nlmsg_new(payload, GFP_KERNEL);
	if(!skb_rep)
	{
		printk("Cannot create reply object.\n");
		return;
	}
	nlh_rep = NLMSG_NEW(skb_rep, NETLINK_CB(skb).pid,
		nlh->nlmsg_seq, 0, payload, 0);
	
	memcpy(nlmsg_data(nlh_rep), nlmsg_data(nlh), payload);
	netlink_unicast(skb->sk, skb_rep, NETLINK_CB(skb).pid, MSG_DONTWAIT);
	return;
	//this mark is needed for use of NLMSG_NEW macro
nlmsg_failure:
	printk("Cannot create reply message.\n");
	kfree_skb(skb_rep);
}
// callback for socket
static void nl_data_ready (struct sk_buff *skb)
{
	struct nlmsghdr	*nlh = nlmsg_hdr(skb);
	printk("%s: received netlink message payload:%s\n",
        __FUNCTION__, (const char*)NLMSG_DATA(nlh));
    
    wake_up_interruptible(skb->sk->sk_sleep);

	my_echo(skb);
	//skb will be freed inside mechanism of netlink sockets
}

static int netlink_test_init(void)
{
	printk(KERN_ALERT "Netlink test modules starts.\n");
	
	nl_sk = netlink_kernel_create(&init_net,
				NETLINK_TEST,
				0,
				nl_data_ready,
				NULL,
				THIS_MODULE);
	return 0;
}

static void netlink_test_exit(void)
{
	netlink_kernel_release(nl_sk);
	printk(KERN_ALERT "Netlink test modules ends.\n");
}

module_init(netlink_test_init);
module_exit(netlink_test_exit);
