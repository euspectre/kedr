/*
 * API for some type of connection between kernel space
 * and user space. This connection may be used
 * (and it is developed for use) in implementing syscall-like functions
 * in user space.
 */

#ifndef SYSCALL_CONNECTOR_H
#define SYSCALL_CONNECTOR_H

	
/*
 * Type, indetified interaction process type
 * between user space and kernel space.
 * 
 * Type which with nl_pid should uniqely identify interaction process
 * between user and kernel spaces.
 */

typedef int interaction_id;



#ifdef __KERNEL__
#include <linux/types.h> /* __u32 */

typedef struct _sc_interaction sc_interaction;

/*
 * Create interaction "channel" with user space.
 */

sc_interaction* sc_interaction_create(__u32 pid, interaction_id type);

__u32 sc_interaction_get_pid(const sc_interaction* interaction);

interaction_id sc_interaction_get_type(const sc_interaction* interaction);

void sc_interaction_destroy(sc_interaction* interaction);

/*
 * Send given message via given interaction.
 */

int sc_send(sc_interaction* interaction, const void *buf, size_t len);

/*
 * Type of callback function for recieve message.
 *
 * interaction - interaction channel, via that message is recieved,
 * buf - array of bytes recieved,
 * len - length of array 'buf',
 * data - some data, supplied when register callback.
 */

typedef void (*sc_recv_callback_type) (sc_interaction* interaction,
	const void* buf, size_t len, void* data);

/*
 * Register/unregister function, which will be called when
 * message of given type is recieved.
 */

int sc_register_callback_for_type(interaction_id type,
	sc_recv_callback_type cb, void* data);

int sc_unregister_callback_for_type(interaction_id type);

/*
 * Register/unregister function, which will be called when
 * message via given interaction is recieved.
 * 
 */

int sc_register_callback(sc_interaction* interaction,
	sc_recv_callback_type cb, void* cb_data);

int sc_unregister_callback(sc_interaction* interaction);

#else /* __KERNEL__ */
#include <linux/types.h> /* __u32 */

typedef struct _sc_interaction sc_interaction;

/*
 * Create interaction "channel" with kernel.
 */

sc_interaction* sc_interaction_create(__u32 pid, interaction_id in_type);

void sc_interaction_destroy(sc_interaction* interaction);

/*
 * Send message to the kernel with content of 'buffer' via 'interaction'.
 */

ssize_t sc_send(sc_interaction* interaction, const void* buf, size_t len);

/*
 * Recieve message from the kernel with content of 'buffer' via 'interaction'.
 */

ssize_t sc_recv(sc_interaction* interaction, void* buf, size_t len);
#endif /* __KERNEL__ */

#endif /* SYSCALL_CONNECTOR_H */
