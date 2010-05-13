/*
 * Implements API for using some sort of "connections" to the kernel
 */

#if __GNUC__ >= 4
#define HELPER_DLL_EXPORT __attribute__((visibility("default")))

#else
#define HELPER_DLL_EXPORT

#endif /* __GNUC__ >= 4 */


#include <string.h> /* memset */
#include <stdlib.h> /* malloc */
#include <stdio.h> /*for printf*/

#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h> /* close() and getpid()*/

#include <errno.h>

#include "../syscall_connector.h"

#include "../syscall_connector_internal.h"

struct _sc_interaction
{
	int sock_fd;
	__u32 pid;
	interaction_id in_type;
	//may be other fields(e.g, sequence)
};

/*
 * Create interaction "process" with kernel (on the user side).
 */

HELPER_DLL_EXPORT sc_interaction*
sc_interaction_create(__u32 pid, interaction_id in_type)
{
	struct sockaddr_nl src_addr;
	sc_interaction* interaction;
	int sock_fd = socket(PF_NETLINK, SOCK_RAW, SC_NETLINK_PROTO);
	if(!sock_fd)
	{
		printf("Cannot create socket.\n");
		return NULL;
	}
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = pid;  /* self pid */
	src_addr.nl_groups = 0;  /* not in mcast groups */
	if(bind(sock_fd, (struct sockaddr*)&src_addr,
	  sizeof(src_addr)) == -1)
	{
		printf("Cannot bind socket.\n");
		close(sock_fd);
		return NULL;
	}
	interaction = malloc(sizeof(*interaction));
	if(!interaction)
	{
		printf("Cannot allocate memory for interaction structure.\n");
		close(sock_fd);
		return NULL;
	}
	interaction->sock_fd = sock_fd;
	interaction->pid = pid;
	interaction->in_type = in_type;
	
	return interaction;
}

HELPER_DLL_EXPORT void
sc_interaction_destroy(sc_interaction* interaction)
{
	close(interaction->sock_fd);
	free(interaction);
}

/*
 * Send message to the kernel with content of 'buffer' via 'interaction'.
 */

HELPER_DLL_EXPORT ssize_t
sc_send(sc_interaction* interaction, const void* buf, size_t len)
{
	ssize_t result;
	struct sc_msg sc_message = {
		.in_type = interaction->in_type,
		.payload = buf,
		.payload_length = len
	};
		
	size_t len_real = sc_msg_len(&sc_message);
	struct sockaddr_nl dest_addr = {
		.nl_family = AF_NETLINK,
		.nl_pid = 0,   /* For Linux Kernel */
		.nl_groups = 0 /* unicast */
	};

	struct iovec iov;
	struct msghdr msg = {
		.msg_name = (void *)&dest_addr,
		.msg_namelen = sizeof(dest_addr),
		.msg_iov = &iov,
		.msg_iovlen = 1
	};
	
	struct nlmsghdr *nlh = (struct nlmsghdr *)malloc(
				 NLMSG_SPACE(len_real));
	/* Fill the netlink message header */
	nlh->nlmsg_len = NLMSG_SPACE(len_real);
	nlh->nlmsg_pid = interaction->pid;	/* sender pid */
	nlh->nlmsg_type = 0; 	/* type - none special */
	nlh->nlmsg_seq = 1; 	/* arbitrary sequence number */
	nlh->nlmsg_flags = 0; 	/* no special flags */
	
	/* Fill in the netlink message payload */
	sc_msg_put(&sc_message, NLMSG_DATA(nlh));
	
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;

	if((result = sendmsg(interaction->sock_fd, &msg, 0)) == -1)
	{
		printf("send returns error: %s\n", strerror(errno));
	}
	free(nlh);
	return result;
}

/*
 * Recieve message from the kernel with content of 'buffer' via 'interaction'.
 */

HELPER_DLL_EXPORT ssize_t
sc_recv(sc_interaction* interaction, void* buf, size_t len)
{
	//fill this struct only for calculate length of full message
	struct sc_msg sc_message = {
		.in_type = interaction->in_type,
		.payload = NULL,
		.payload_length = len
	};
	struct iovec iov;
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1
	};
	struct nlmsghdr *nlh = (struct nlmsghdr *)malloc(
				 NLMSG_SPACE(sc_msg_len(&sc_message)));
	
	iov.iov_base = (void *)nlh;
	iov.iov_len = NLMSG_SPACE(sc_msg_len(&sc_message));
	
	if(recvmsg(interaction->sock_fd, &msg, 0) == -1)
	{
		printf("recieve returns error: %s", strerror(errno));
		return -1;
	}
	if(sc_msg_get(&sc_message, NLMSG_DATA(nlh), NLMSG_PAYLOAD(nlh, 0)))
	{
		printf("Incorrect format of the message recieved.\n");
		free(nlh);
		return -1;
	}
	if(interaction->in_type != sc_message.in_type)
	{
		printf("Message of unexpected type is recieved.\n");
		free(nlh);
		return 0;
		//peek message back to the message queue
		//will be implemented in the future
		//(using MSG_PEEK flag in recvmsg)
	}
	memcpy(buf, sc_message.payload, sc_message.payload_length);
	
	free(nlh);
	
	return sc_message.payload_length;
}