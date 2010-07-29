/*
 * API for some type of connection between kernel space
 * and user space. This connection may be used
 * (and it is developed for use) in implementing syscall-like functions
 * in user space.
 *
 * Important, that sc_send() from user space will not returned until the callback,
 * registered for this interaction in the kernel space, is completed.
 *
 * Also important, that unregistering of callbacks for interaction type
 * or for concrete interaction channel does not garanteed, that this callback is not executed at that moment.

 */

#ifndef SYSCALL_CONNECTOR_H
#define SYSCALL_CONNECTOR_H

/*
 * Type, indetified interaction process type
 * between user space and kernel space.
 * 
 * Type which with some __u32 value should uniqely identify interaction process
 * between user and kernel spaces.
 */

typedef int sc_interaction_id;

/*
 * Interfaces for kernel space.
 */

#ifdef __KERNEL__
#include <linux/types.h> /* __u32 */

typedef struct _sc_interaction sc_interaction;

/*
 * Create interaction channel with user space.
 */

sc_interaction* sc_interaction_create(sc_interaction_id type, __u32 pid);

/*
 * Simple getters.
 */

sc_interaction_id sc_interaction_get_type(const sc_interaction* interaction);

__u32 sc_interaction_get_pid(const sc_interaction* interaction);

/*
 * Destroy interaction channel and free all resources, binded with it.
 */

void sc_interaction_destroy(sc_interaction* interaction);

/*
 * Send given message, as 'len' bytes, started with 'buf', via given interaction channel.
 */

int sc_send(sc_interaction* interaction, const void *buf, size_t len);

/*
 * Type of callback function for recieve message.
 *
 * interaction - interaction channel, via that message is recieved,
 * buf - array of bytes recieved,
 * len - length of array 'buf',
 * data - some data, supplied when register callback.
 *
 * When this function returns, 'buf' became invalid.
 */

typedef void (*sc_recv_callback_type) (sc_interaction* interaction,
	const void* buf, size_t len, void* data);

/*
 * Register function, which will be called when
 * message of given type is recieved.
 */

int sc_register_callback_for_type(sc_interaction_id type,
	sc_recv_callback_type cb, void* cb_data);

/*
 * Unregister callback function, making all futher calls to
 * the channel of given type
 */

int sc_unregister_callback_for_type(sc_interaction_id type);

/*
 * Register/unregister function, which will be called when
 * message via given interaction channel is recieved.
 * 
 */

int sc_register_callback_for_channel(sc_interaction* interaction,
	sc_recv_callback_type cb, void* cb_data);

int sc_unregister_callback_for_channel(sc_interaction* interaction);

/*
 * Interface for user space.
 */

#else /* __KERNEL__ */
#include <linux/types.h> /* __u32 */

typedef struct _sc_interaction sc_interaction;

/*
 * Create interaction channel with the kernel.
 */

sc_interaction* sc_interaction_create(sc_interaction_id in_type, __u32 pid);

/*
 * Destroy interaction channel and free all resources, binded with it.
 */

void sc_interaction_destroy(sc_interaction* interaction);

/*
 * Send given message, as 'len' bytes, started with 'buf', via given interaction channel to the kernel.
 */

ssize_t sc_send(sc_interaction* interaction, const void* buf, size_t len);

/*
 * Recieve message from the kernel via given interaction channel.
 *
 */

ssize_t sc_recv(sc_interaction* interaction, void* buf, size_t len);
#endif /* __KERNEL__ */

#endif /* SYSCALL_CONNECTOR_H */
