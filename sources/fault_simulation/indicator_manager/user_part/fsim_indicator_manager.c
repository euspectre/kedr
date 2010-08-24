#if __GNUC__ >= 4
#define FSIM_HELPER_DLL_EXPORT __attribute__((visibility("default")))

#else
#define FSIM_HELPER_DLL_EXPORT

#endif /* __GNUC__ >= 4 */


#include <stdlib.h> /* malloc */
#include <stdio.h> /*for printf*/

#include <unistd.h> /* close() and getpid()*/

#include <errno.h>
// Connector API
#include <kedr/syscall_connector/syscall_connector.h>
// Protocol definitions
#include <kedr/fault_simulation/fsim_indicator_manager_internal.h>

#define MAX_PAYLOAD 1024

#define MESSAGE_PID getpid()
//interaction type for set_indicator
static sc_interaction_id kedr_fsim_set_indicator_id = -1;

//Auxuliary functions.

/*
 * Set indicator for particular simulation point.
 * 
 * 'user_data' array of bytes is used as additional parameter(s)
 * for indicator function. In common cases user_data may be NULL
 * (user_data_len should be 0 in that case).
 * 
 * On success returns 0.
 * If point_name or indicator_name don't exist, or user_data array
 * has incorrect format for given indicator, returns not 0.
 */

FSIM_HELPER_DLL_EXPORT int
kedr_fsim_set_indicator(const char* point_name,
	const char* indicator_name, void* params, size_t params_len)
{
	sc_interaction* interaction;
	struct kedr_fsim_set_indicator_payload send_payload;
	
	send_payload.point_name = point_name,
	send_payload.indicator_name = indicator_name,
	send_payload.params = params,
	send_payload.params_len = params_len;
	
	struct kedr_fsim_set_indicator_reply recv_payload;
	
	void  *message, *recv_message = NULL;
	size_t message_size, recv_message_size = MAX_PAYLOAD;
	
	message_size = kedr_fsim_set_indicator_payload_len(&send_payload);
	message = malloc(message_size);
	kedr_fsim_set_indicator_payload_put(&send_payload, message);
	
	
	interaction = sc_interaction_create(MESSAGE_PID,
		kedr_fsim_set_indicator_id);
	if(!interaction)
	{
		printf("Cannot create interaction of type "
			"kedr_fsim_set_indicator_id.\n");
		free(message);
		return -1;
	}
	if(sc_send(interaction, message, message_size) <= 0)
	{
		printf("Cannot send message.\n");
		free(message);
		sc_interaction_destroy(interaction);
		return -1;
	}
	free(message);
	
	recv_message = malloc(recv_message_size);
	
	sc_recv(interaction, recv_message, recv_message_size);
	sc_interaction_destroy(interaction);
	
	kedr_fsim_set_indicator_reply_get(&recv_payload,
		recv_message, recv_message_size);

	free(recv_message);
	
	return recv_payload.result;
}

/////////////////////////////////////////////////////
void __attribute__((constructor)) my_init(void)
{
	printf("Constructor was called.\n");
	if(sc_library_try_use(fsim_library_name, MESSAGE_PID,
        &kedr_fsim_set_indicator_id, sizeof(kedr_fsim_set_indicator_id)))
	{
		printf("Cannot initialize communication with kernel module:"
				"perhaps it isn't loaded.\n");
		printf("fault simulation library initialization failed.\n");
		exit(1);
	}
}
void __attribute__((destructor)) my_fini(void)
{
	printf("Destructor was called.\n");
	sc_library_unuse(fsim_library_name, MESSAGE_PID);
	kedr_fsim_communication_break();
}
