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

#include <linux/fcntl.h> /* fcntl constants */

#include <errno.h>

#include <kedr/syscall_connector/syscall_connector.h>
#include <kedr/syscall_connector/syscall_connector_internal.h>

#define debug(str, ...) printf("Debug: %s: " str "\n", __func__, __VA_ARGS__)
#define debug0(str) debug("%s", str)

#define print_error(str, ...) printf("Error: %s: " str "\n", __func__, __VA_ARGS__)
#define print_error0(str) print_error("%s", str)

struct _sc_interaction
{
	sc_interaction_id in_type;
	__u32 pid;

	int sock_fd;
	//may be other fields(e.g, sequence)
};

/*
 * Create interaction "process" with kernel (on the user side).
 */

HELPER_DLL_EXPORT sc_interaction*
sc_interaction_create(sc_interaction_id in_type, __u32 pid)
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
sc_interaction_set_flags(sc_interaction* interaction, long flags)
{
    long socket_flags = fcntl(interaction->sock_fd, F_GETFL);
    if(flags | SC_INTERACTION_NONBLOCK)
        socket_flags |= O_NONBLOCK;
    else
        socket_flags &= ~O_NONBLOCK;
    fcntl(interaction->sock_fd, F_SETFL, socket_flags);
}

HELPER_DLL_EXPORT long
sc_interaction_get_flags(sc_interaction* interaction)
{
    long flags = 0;
    long socket_flags = fcntl(interaction->sock_fd, F_GETFL);
    if(socket_flags |= O_NONBLOCK)
        flags |= SC_INTERACTION_NONBLOCK;
    else
        flags |= ~SC_INTERACTION_NONBLOCK;
    return flags;
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
	nlh->nlmsg_type = SC_NLMSG_TYPE; 	/* type - our type */
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
	return (result == -1) ? result : len;
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
		printf("recieve returns error: %s\n", strerror(errno));
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

//Some implementations of syscalls

/*
 * Mark module which contatin implementation of kernel part as used,
 * in such way prevent it from unexpected unloading.
 */

static int sc_module_try_use(__u32 pid)
{
    char buf[sizeof(GLOBAL_USAGE_SERVICE_MSG_REPLY)] = "";
    int result = 0;
    
    sc_interaction* interaction = sc_interaction_create(GLOBAL_USAGE_SERVICE_IT, pid);
    if(!interaction) return 1;//fail to create interaction for some reason
    
    sc_interaction_set_flags(interaction, SC_INTERACTION_NONBLOCK);
    if(sc_send(interaction, GLOBAL_USAGE_SERVICE_MSG_USE, sizeof(GLOBAL_USAGE_SERVICE_MSG_USE))
        != sizeof(GLOBAL_USAGE_SERVICE_MSG_USE))
    {
        //failed to send
        print_error0("Error occures while sending message.");
        result = 1;
    }
    else if(sc_recv(interaction, buf, sizeof(buf)) != sizeof(buf))
    {
        //for some reason we cannot prevent kernel module from unload
        print_error0("Error occures while recieve message.");
        result = 1;
    }
    else if(memcmp(buf, GLOBAL_USAGE_SERVICE_MSG_REPLY, sizeof(buf)) != 0)
    {
        //incorrect message was recieved
        print_error0("Incorrect message content was recieved.");
        result = 1;
    }
    else
    {
        debug0("Module is used now.");
    }

    sc_interaction_destroy(interaction);
    return result;
}

/*
 * Mark module as unused.
 */

static int sc_module_unuse(__u32 pid)
{
    sc_interaction* interaction = sc_interaction_create(GLOBAL_USAGE_SERVICE_IT, pid);
    if(!interaction) return;//fail to create interaction for some reason
    
    sc_send(interaction, GLOBAL_USAGE_SERVICE_MSG_UNUSE, sizeof(GLOBAL_USAGE_SERVICE_MSG_UNUSE));
    debug0("Module is unused now.");
    sc_interaction_destroy(interaction);
}


/*
 * Mark library with given name as used, in such way prevent it from unexpected unloading.
 *
 * On success, return 0 and copy message, which contain information,
 * specific for given library, into the buffer.
 *
 * Otherwise returns not 0.
 *
 */

HELPER_DLL_EXPORT int
sc_library_try_use(const char* library_name, __u32 pid, void* buf, size_t len)
{
    struct sc_named_libraries_send_msg send_msg;
    void* send_buf;
    size_t send_len;

    int result = 0;
    
    sc_interaction* interaction = sc_interaction_create(NAMED_LIBRARIES_SERVICE_IT, pid);
    if(!interaction) return 1;//fail to create interaction for some reason
    
    sc_interaction_set_flags(interaction, SC_INTERACTION_NONBLOCK);
    
    send_msg.control = 1;
    send_msg.library_name = library_name;
    
    send_len = sc_named_libraries_send_msg_len(&send_msg);
    
    if((send_buf = malloc(send_len)) == NULL)
    {
        //failed to allocate buffer
        print_error0("Cannot allocate memory for buffer.");
        result = 1;
    }
    if(result)
    {
        sc_interaction_destroy(interaction);
        return result;
    }
    sc_named_libraries_send_msg_put(&send_msg, send_buf);
    if(sc_send(interaction, send_buf, send_len) != send_len)
    {
        //failed to send
        print_error0("Error occures while sending message.");
        result = 1;
    }
    else if(sc_recv(interaction, buf, len) != len)
    {
        //for some reason we cannot prevent kernel module from unload
        print_error0("Error occures while recieve message.");
        result = 1;
    }
    else
    {
        debug("Library '%s' is used now.", library_name);
    }
    free(send_buf);
    sc_interaction_destroy(interaction);
    return result;
}

/*
 * Mark library as unused.
 */

HELPER_DLL_EXPORT void
sc_library_unuse(const char* library_name, __u32 pid)
{
    struct sc_named_libraries_send_msg send_msg;
    void* send_buf;
    size_t send_len;

    int result = 0;
    
    sc_interaction* interaction = sc_interaction_create(NAMED_LIBRARIES_SERVICE_IT, pid);
    if(!interaction) return;//fail to create interaction for some reason
    
    sc_interaction_set_flags(interaction, SC_INTERACTION_NONBLOCK);
    
    send_msg.control = 0;
    send_msg.library_name = library_name;
    
    send_len = sc_named_libraries_send_msg_len(&send_msg);
    
    if((send_buf = malloc(send_len)) == NULL)
    {
        //failed to allocate buffer
        print_error0("Cannot allocate memory for buffer.");
        result = 1;
    }
    if(result)
    {
        sc_interaction_destroy(interaction);
        return;
    }
    sc_named_libraries_send_msg_put(&send_msg, send_buf);
    if(sc_send(interaction, send_buf, send_len) != send_len)
    {
        //failed to send
        print_error0("Error occures while sending message.");
        result = 1;
    }
    else
    {
        debug("Library '%s' is unused now.", library_name);
    }
    free(send_buf);
    sc_interaction_destroy(interaction);

    return;

}


/////////////////////////Constructor and destructor of library/////////////////////
static void __attribute__ ((constructor))
syscall_connector_init(void) 
{
    if(sc_module_try_use(getpid()))
        exit(1);//fail to connect with kernel module, or fail to use it.
}

static void __attribute__ ((destructor))
syscall_connector_destroy(void)
{
    sc_module_unuse(getpid());
}