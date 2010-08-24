/*
 * Protocol between kernel and user definitions.
 */

#ifndef SYSCALL_CONNECTOR_INTERNAL_H
#define SYSCALL_CONNECTOR_INTERNAL_H

#define SC_NETLINK_PROTO 17
//'nlmsg_type' field in the 'struct nlmsghdr' of the message
#define SC_NLMSG_TYPE (NLMSG_MIN_TYPE + 7)
//interaction type of global usage service
#define GLOBAL_USAGE_SERVICE_IT 0
//interaction type of named libraries service
#define NAMED_LIBRARIES_SERVICE_IT 1

#ifdef __KERNEL__
#include <linux/string.h> /* memcpy */
#include <linux/types.h> /* __u32 */
#else
#include <string.h> /* memcpy */
#include <linux/types.h> /* __u32 */
#endif

/*
 *  Write variable 'var' of type 'var_type' to the 'message' array.
 *  Also advance 'message' pointer.
 */

#define SC_MESSAGE_WRITE(message, var, var_type) \
	*((var_type*)message) = var;\
	message = (var_type*)message + 1


/*
 * Same but for array of bytes.
 */

#define SC_MESSAGE_WRITE_BYTES(message, bytes, nbytes) \
	memcpy(message, bytes, nbytes);\
	message = (char*)message + nbytes;
	

/*
 *  Read variable 'var' of type 'var_type' from the 'message' array.
 *  Also advance 'message' pointer.
 * 
 *  'message_len' is used to verify, that it is at least as variable size.
 *  If it is not, "return -1;".
 *  Also 'message_len' is decremented, so 'message' and 'message_len'
 *  describe the rest of the message.
 */

#define SC_MESSAGE_GET(message, message_len, var, var_type) \
	if(message_len < sizeof(var_type)) return -1;\
	var = *(const var_type*)message;\
	message = (const var_type*)message + 1;\
	message_len -= sizeof(var_type);

/*
 * Similar but for array of bytes.
 * 
 * After call, 'bytes' points to the beginning of the message.
 */

#define SC_MESSAGE_GET_BYTES(message, message_len, bytes, nbytes) \
	if(message_len < nbytes) return -1;\
	bytes = message;\
	message = (const char*)message + nbytes;\
	message_len -= nbytes;

/*
 * Formats of the message:
 *
 * | sc_interaction_id | payload_length | payload | [padding] |
 *                                   \_________/
 *                                  payload_length
 */

struct sc_msg
{
	sc_interaction_id in_type;
	size_t payload_length;
	const void* payload;
};

static inline size_t sc_msg_len(struct sc_msg* msg)
{
	return sizeof(msg->in_type) + sizeof(msg->payload_length)
		+ msg->payload_length;
}

static inline void sc_msg_put(const struct sc_msg* msg, void* buffer)
{
	SC_MESSAGE_WRITE(buffer, msg->in_type, sc_interaction_id);
	SC_MESSAGE_WRITE(buffer, msg->payload_length, size_t);
	SC_MESSAGE_WRITE_BYTES(buffer, msg->payload, msg->payload_length);
}
static inline int sc_msg_get(struct sc_msg* msg, const void* buffer,
	size_t buffer_len)
{
	SC_MESSAGE_GET(buffer, buffer_len, msg->in_type, sc_interaction_id);
	SC_MESSAGE_GET(buffer, buffer_len, msg->payload_length, size_t);
	SC_MESSAGE_GET_BYTES(buffer, buffer_len,
		msg->payload, msg->payload_length);
	return 0;
}
/*
 * Global usage service.
 *
 * user space: "use"
 * kernel space: "ok"
 * -> prevent module from unload
 * user space: "use"
 * kernel space: (nothing)
 * ->failed to prevent module from unload
 * user space: "unuse"
 * ->module may be unloaded now
 */

static const char GLOBAL_USAGE_SERVICE_MSG_USE[] = "use";
static const char GLOBAL_USAGE_SERVICE_MSG_REPLY[] = "ok";
static const char GLOBAL_USAGE_SERVICE_MSG_UNUSE[] = "unuse";

/*
 * Named libraries service.
 *
 * user space: "use"|"library A"
 * kernel space: "ok", "adresses count", "adresses"
 * ->prevent unloading of library "library A"
 * user space: "use"|"library A"
 * kernel space: (nothing) 
 * ->failed to prevent library "library A" from unload
 * user space: "unuse"|"library A"
 * library "library A" may be unloaded now
 */

static const char NAMED_LIBRARIES_SERVICE_MSG_USE[] = "use";
static const char NAMED_LIBRARIES_SERVICE_MSG_REPLY[] = "ok";
static const char NAMED_LIBRARIES_SERVICE_MSG_UNUSE[] = "unuse";

struct sc_named_libraries_send_msg
{
    int control;// 1-use, 0-unuse
    const char* library_name;
};

static inline size_t sc_named_libraries_send_msg_len(struct sc_named_libraries_send_msg* msg)
{
	return sizeof(msg->control) + strlen(msg->library_name) + 1/*'\0'*/;
}

static inline void sc_named_libraries_send_msg_put(const struct sc_named_libraries_send_msg* msg, void* buffer)
{
	SC_MESSAGE_WRITE(buffer, msg->control, int);
	SC_MESSAGE_WRITE_BYTES(buffer, msg->library_name, strlen(msg->library_name));
    SC_MESSAGE_WRITE(buffer, '\0', char);
}
static inline int sc_named_libraries_send_msg_get(struct sc_named_libraries_send_msg* msg, const void* buffer,
	size_t buffer_len)
{
	SC_MESSAGE_GET(buffer, buffer_len, msg->control, int);
	SC_MESSAGE_GET_BYTES(buffer, buffer_len,
		msg->library_name, buffer_len - 1);
	return *((char*)buffer) == '\0';
}

#endif /* SYSCALL_CONNECTOR_INTERNAL_H */