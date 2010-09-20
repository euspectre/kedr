#ifndef KEDR_KEI_COMMON_H
#define KEDR_KEI_COMMON_H

#ifdef __KERNEL__
#include <linux/string.h> /*memcpy, strlen*/
#else
#include <string.h> /*memcpy, strlen*/
#endif

/*
 *  Write variable 'var' of type 'var_type' to the 'message' array.
 *  Also advance 'message' pointer.
 */

#define KEDR_FSIM_MESSAGE_WRITE(message, var, var_type) \
	*((var_type*)message) = var;\
	message = (var_type*)message + 1


/*
 * Same but for array of bytes.
 */

#define KEDR_FSIM_MESSAGE_WRITE_BYTES(message, bytes, nbytes) \
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

#define KEDR_FSIM_MESSAGE_GET(message, message_len, var, var_type) \
	if(message_len < sizeof(var_type)) return -1;\
	var = *(const var_type*)message;\
	message = (const var_type*)message + 1;\
	message_len -= sizeof(var_type);

/*
 * Similar but for array of bytes.
 * 
 * After call, 'bytes' points to the beginning of the message.
 */

#define KEDR_FSIM_MESSAGE_GET_BYTES(message, message_len, bytes, nbytes) \
	if(message_len < nbytes) return -1;\
	bytes = message;\
	message = (const char*)message + nbytes;\
	message_len -= nbytes;

/*
 * sys call: kedr_fsim_set_indicator
 * 
 * user space:
 * 1. Forms struct kedr_fsim_set_indicator_payload from function parameters.
 * 2. Create message from it and send to the kernel.
 * kernel space:
 * 3. Recieve message from user space.
 * 4. Retrive struct kedr_fsim_set_indicator_payload from message.
 * 5. Call kedr_fsim_set_indicator_by_name
 * 6. Forms struct kedr_fsim_set_indicator_reply from result.
 * 7. Create message from it and reply with it to the sender.
 * user space:
 * 8. Recieve message from kernel.
 * 9. Retrieve struct kedr_fsim_set_indicator_reply from it.
 * 10. Return value of the 'result' field.
 * 
 */

const char* fsim_library_name = "fsim_set_indicator";


struct kedr_fsim_set_indicator_payload
{
	const char* point_name;
	const char* indicator_name;
	const void* params;
	size_t params_len;
};

/*
 * Format of message, contained struct kedr_fsim_set_indicator_payload:
 * 
 * | 'point_name' (with '\0') | 'indicator_name' (with '\0') | params |
 */

inline static size_t
kedr_fsim_set_indicator_payload_len(
	const struct kedr_fsim_set_indicator_payload* payload)
{
	size_t function_name_len = strlen(payload->point_name) + 1;
	size_t indicator_name_len = strlen(payload->indicator_name) + 1;
	return function_name_len
		+ indicator_name_len
		+ payload->params_len;
}
inline static void
kedr_fsim_set_indicator_payload_put(
	const struct kedr_fsim_set_indicator_payload* payload,
	void* buf)
{
	size_t point_name_len = strlen(payload->point_name) + 1;
	size_t indicator_name_len = strlen(payload->indicator_name) + 1;
	KEDR_FSIM_MESSAGE_WRITE_BYTES(buf, payload->point_name,
		point_name_len);
	KEDR_FSIM_MESSAGE_WRITE_BYTES(buf, payload->indicator_name,
		indicator_name_len);
	KEDR_FSIM_MESSAGE_WRITE_BYTES(buf, payload->params,
		payload->params_len);
}
inline static int
kedr_fsim_set_indicator_payload_get(
	struct kedr_fsim_set_indicator_payload* payload,
	const void* buf,
	size_t buf_len)
{
	size_t indicator_name_len, point_name_len;
	point_name_len = strlen(buf) + 1;
	KEDR_FSIM_MESSAGE_GET_BYTES(buf, buf_len, payload->point_name,
		point_name_len);
	indicator_name_len = strlen(buf) + 1;
	KEDR_FSIM_MESSAGE_GET_BYTES(buf, buf_len, payload->indicator_name,
		indicator_name_len);
	payload->params_len = buf_len;
	KEDR_FSIM_MESSAGE_GET_BYTES(buf, buf_len, payload->params,
		payload->params_len);
	return 0;
}

struct kedr_fsim_set_indicator_reply
{
	int result;
};

/*
 * Format of message, contained struct kedr_fsim_set_indicator_reply:
 * 
 * 'result'
 */

inline static size_t
kedr_fsim_set_indicator_reply_len(
	const struct kedr_fsim_set_indicator_reply* reply)
{
	// message len doesn't depend of 'reply' content,
	// so mark 'reply' unused.
	(void) *reply; 
	return sizeof(int);
}
inline static void
kedr_fsim_set_indicator_reply_put(
	const struct kedr_fsim_set_indicator_reply* reply,
	void* buf)
{
	KEDR_FSIM_MESSAGE_WRITE(buf, reply->result, int);
}

inline static int
kedr_fsim_set_indicator_reply_get(
	struct kedr_fsim_set_indicator_reply* reply,
	const void* buf,
	size_t buf_len)
{
	KEDR_FSIM_MESSAGE_GET(buf, buf_len, reply->result, int);
	return 0;
}

/*
 * Encoding array of strings to the parameters of 'init_state'
 * function for indicator.
 *
 * |argc|str1_offset|...|strn_offset|str1|...|strn|
 */

inline static size_t
kedr_fsim_indicator_params_strings_len(int argc,
    const char const** argv)
{
    int i;
    size_t result = sizeof(int) + argc * sizeof(int);
    for(i = 0; i < argc; i++) 
        result += strlen(argv[i]) + 1;
    return result;
}

inline static size_t
kedr_fsim_indicator_params_strings_put(int argc,
    const char const** argv, void* buf)
{
    int i;
    int offset;
    KEDR_FSIM_MESSAGE_WRITE(buf, argc, int);
    for(i = 0, offset = sizeof(int) + argc * sizeof(int);
        i < argc; offset +=strlen(argv[i]) + 1, i++)
    {
        KEDR_FSIM_MESSAGE_WRITE(buf, offset, int);
    }
    for(i = 0; i < argc; i++)
    {
        KEDR_FSIM_MESSAGE_WRITE_BYTES(buf, argv[i], strlen(argv[i]) + 1);
    }
    return 0;
}

struct kedr_fsim_indicator_params_strings
{
    int argc;
    const void* buf;
};

//getter of string
inline const char*
kedr_fsim_indicator_params_strings_get_string(
    const struct kedr_fsim_indicator_params_strings* arr,
    int i)
{
    if((i < 0) || (i >= arr->argc)) return NULL;
    return arr->buf + ((int*)(arr->buf + sizeof(int)))[i];
}

inline static size_t
kedr_fsim_indicator_params_strings_get(
    struct kedr_fsim_indicator_params_strings* arr,
    const void* buf, size_t buf_len)
{
    int i, argc;
    int prev_offset, *poffset;
    if(buf_len < sizeof(int)) return -1;

    argc = *((int*)buf);
    
    if(argc < 0)
        return -1;//number of strings shouldn't be negative
    if(argc == 0)
    {
        if (buf_len != sizeof(int)) return -1;
        //no strings - this is correct
        arr->buf = NULL;
        arr->argc = argc;
        return 0;
    }
    poffset = (int*)(buf + sizeof(int));
    prev_offset = *poffset;
    if(prev_offset != sizeof(int) + argc * sizeof(int))
        return -1;//incorrect offset of first string
    for(i = 1, ++poffset; i < argc; i++, ++poffset)
    {
        int offset = *poffset;
        if(offset <= prev_offset)
            return -1;//incorrect order of string offset
        if(offset >= buf_len)
            return -1;//out of boundaries
        if(strnlen(buf + prev_offset, offset-prev_offset) != (offset-prev_offset - 1))
            return -1;//incorrect length of string
        prev_offset = offset;
    }
    if(strnlen(buf + prev_offset, buf_len - prev_offset) != (buf_len - prev_offset - 1))
        return -1;//incorrect length of string
    arr->argc = argc;
    arr->buf = buf;
    return 0;
}

#endif /* KEDR_KEI_COMMON_H */
