#include <string.h> /* memset */
#include <stdlib.h> /* malloc */
#include <stdio.h> /*for printf*/

#include <sys/socket.h>
#include <linux/netlink.h>

#include <errno.h>

#define MAX_PAYLOAD 1024  /* maximum payload size*/

#define NETLINK_TEST 17
// Send first 'len' bytes from data to the kernel via socket sock_fd.
// Returns number of transmitted bytes or -1.
static ssize_t my_send_msg_to_kernel(int sock_fd,
	const void* data, size_t len)
{
	ssize_t result;
	struct sockaddr_nl src_addr, dest_addr = {
		.nl_family = AF_NETLINK,
		.nl_pid = 0,   /* For Linux Kernel */
		.nl_groups = 0 /* unicast */
	};
	socklen_t addr_len = sizeof(src_addr);
	struct iovec iov = {};
	struct msghdr msg = {
		.msg_name = (void *)&dest_addr,
		.msg_namelen = sizeof(dest_addr),
		.msg_iov = &iov,
		.msg_iovlen = 1
	};
	
	struct nlmsghdr *nlh = (struct nlmsghdr *)malloc(
				 NLMSG_SPACE(len));
	/* Fill the netlink message header */
	nlh->nlmsg_len = NLMSG_SPACE(len);
	getsockname(sock_fd, (struct sockaddr*)&src_addr, &addr_len);
	nlh->nlmsg_pid = src_addr.nl_pid;	/* sender pid */
	nlh->nlmsg_type = 0; 	/* type - none special */
	nlh->nlmsg_seq = 1; 	/* arbitrary sequence number */
	nlh->nlmsg_flags = 0; 	/* no special flags */
	
	/* Fill in the netlink message payload */
	memcpy(NLMSG_DATA(nlh), data, len);
	
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;

	if((result = sendmsg(sock_fd, &msg, 0)) == -1)
	{
		printf("send returns error: %s\n", strerror(errno));
	}
	free(nlh);
	return result;
}
/* 	Recieve message via the socket.
 *  Message is returned by function and should be freed
 * 	when no longer needed.
 *  'len' is filled by function with actual length of message.
 */ 
static void* my_recv_msg(int sock_fd,
	size_t *len)
{
	struct iovec iov;
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1
	};
	void* result;

	struct nlmsghdr *nlh = (struct nlmsghdr *)malloc(
				 NLMSG_SPACE(MAX_PAYLOAD));
	
	iov.iov_base = (void *)nlh;
	iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);
	
	if(recvmsg(sock_fd, &msg, 0) == -1)
	{
		printf("recieve returns error: %s", strerror(errno));
		return NULL;
	}
	
	result = malloc(NLMSG_PAYLOAD(nlh, 0));
	memcpy(result, NLMSG_DATA(nlh), NLMSG_PAYLOAD(nlh, 0));
	
	free(nlh);
	return result;
}

int main(int argc, char**argv)
{
	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr *nlh = NULL;
	struct msghdr msg;
	struct iovec iov;
	int sock_fd;
	size_t recv_len;
	void *recv_message = NULL;

	if(argc != 2)
	{
		printf("Usage: test message\n");
		return 1;
	}
	const char* message = argv[1];
	size_t message_size = strlen(message) + 1;
	
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_TEST);

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();  /* self pid */
	src_addr.nl_groups = 0;  /* not in mcast groups */
	bind(sock_fd, (struct sockaddr*)&src_addr,
	  sizeof(src_addr));

	my_send_msg_to_kernel(sock_fd, message, message_size);
	my_recv_message = recv_msg(sock_fd, &recv_len);
	
	printf("Received message: \"%s\".\n",
			(const char*)recv_message);
	free(recv_message);

	/* Close Netlink Socket */
	close(sock_fd);
	
	return 0;
}
