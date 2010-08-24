/*
 * API for some type of connection between kernel space
 * and user space. This connection may be used
 * (and it is developed for use) in implementing syscall-like functions
 * in user space.
 *
 * Important, that sc_send() from user space will not returned until the callback,
 * registered for this interaction in the kernel space, is completed.
 *
 * Unregistering callback for interaction type firstly unregisters all channel callbacks
 * for interactions of given type. 
 * Every such unregistering is wait until all current calls to the callback is returned.
 *
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

// Channels created, when callback function for type is called
/*
 * Create interaction channel with user space.
 */

//sc_interaction* sc_interaction_create(sc_interaction_id type, __u32 pid);

/*
 * Simple getters.
 */

sc_interaction_id sc_interaction_get_type(const sc_interaction* interaction);

__u32 sc_interaction_get_pid(const sc_interaction* interaction);

// commented out for the same reason, as sc_interaction_create.
/*
 * Destroy interaction channel and free all resources, binded with it.
 */

//void sc_interaction_destroy(sc_interaction* interaction);

/*
 * Send given message, as 'len' bytes, started with 'buf', via given interaction channel.
 */

int sc_send(const sc_interaction* interaction, const void *buf, size_t len);

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

typedef void (*sc_recv_callback_t) (const sc_interaction* interaction,
	const void* buf, size_t len, void* data);

/*
 * Register function, which will be called when
 * message of given type is recieved.
 *
 * 'destroy' function, if not NULL, will be called with 'data' parameter,
  * when it will be not used.
 */

int sc_register_callback_for_type(sc_interaction_id type,
	sc_recv_callback_t cb, void* data, void (*destroy)(void*));

/*
 * Look for type, for which no callback function is registered,
 * register given function for this type, and return this type.
 *
 * On error return -1.
 */

sc_interaction_id sc_register_callback_for_unused_type(
	sc_recv_callback_t cb, void* data, void (*destroy)(void*));


/*
 * Unregister callback function, making all futher messages via interactions
 * of given type invalid.
 *
 * If 'need_wait' is not 0, also wait, until all invocations of callbacks for this type,
 * and channels with interactions of this type, are returned and it's 'data' was destroyed.
 */

int sc_unregister_callback_for_type(sc_interaction_id type, int need_wait);

/*
 * Register function, which will be called INSTEAD of callback for type,
 * when message via given interaction channel is recieved.
 *
 * Note, that one can register functions for interactions of type,
 * for which callback function is currently exist.
 * Also, callback function for channel will be automatically unregistered,
 * when callback function for type, corresponding to this channel, is unregistered.
 */

int sc_register_callback_for_channel(const sc_interaction* interaction,
	sc_recv_callback_t cb, void* data, void (*destroy)(void*));

/*
 * Unregister callback function, making all futher messages via given channel
 * is processed by the callback for type of this interaction.
 *
 */

int sc_unregister_callback_for_channel(const sc_interaction* interaction);

/////////////////////////////////////////////////////////////////////////
//Some implementations of syscalls

/*
 * Register library, for which defined try_use/unuse operations.
 *
 * try_use() operation should prevent(in some sence) library from unload, and return 0.
 * If it cannot do this for some reason, it should return not 0.
 *
 * unuse() should revert result of use() call.
 *
 * Note, that try_use() may be called many times, and unuse() should revert
 * only one call of successfull try_use.
 *
 * 'reply_msg' will be content of reply message after successfull try_use() call.
 * It may contain, e.g., interaction types for syscalls.
 */

int 
sc_library_register(const char* library_name,
    int (*try_use)(void), void (*unuse)(void),
    void* reply_msg, size_t reply_msg_len
);

/*
 * Unregister library.
 *
 * This call firstly make all futher calls try_use for library to return error
 * (independently from library's try_use method).
 *
 * Then it block calling process, until all previous calls try_use() will have
 * their unuse() pair.
 */

void sc_library_unregister(const char* library_name);
    
/*
 * Interface for user space.
 */

#else /* __KERNEL__ */
#include <linux/types.h> /* __u32 */
#include <sys/types.h> /*size_t, ssize_t, NULL*/

typedef struct _sc_interaction sc_interaction;

#define SC_INTERACTION_NONBLOCK ((long)1)

/*
 * Create interaction channel with the kernel.
 */

sc_interaction* sc_interaction_create(sc_interaction_id in_type, __u32 pid);

/*
 * Set and get flags for interaction.
 *
 * Available flags:
 *  -SC_INTERACTION_NONBLOCK - sc_recv return immediatle, if there are no messages
 *                             in the input queue. By default this flag is cleared.
 */

void
sc_interaction_set_flags(sc_interaction* interaction, long flags);

long 
sc_interaction_get_flags(sc_interaction* interaction);

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
 */

ssize_t sc_recv(sc_interaction* interaction, void* buf, size_t len);

/////////////////////////////////////////////////////////////////////////
//Some implementations of syscalls

/*
 * In the next functions 'pid' should uniqly identify process of unteraction.
 *
 * Note, that it doesn't need to be same in try_use() and unuse() pair.
 */

/*
 * Mark library with given name as used, in such way prevent it from unexpected unloading.
 *
 * On success, return 0 and copy message, which contain information,
 * specific for given library, into the buffer.
 *
 * Otherwise returns not 0.
 *
 * This library should previously be registered in the kernel with sc_library_register().
 *
 */

int sc_library_try_use(const char* library_name, __u32 pid, void* buf, size_t len);

/*
 * Mark library as unused.
 */

void sc_library_unuse(const char* library_name, __u32 pid);

#endif /* __KERNEL__ */

#endif /* SYSCALL_CONNECTOR_H */
