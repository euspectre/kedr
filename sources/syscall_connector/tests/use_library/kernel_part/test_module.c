#include <kedr/syscall_connector/syscall_connector.h>
#include "../common.h"

#include <linux/module.h>
    
void test_service(const sc_interaction* interaction, const void* msg, size_t msg_len, void* data)
{
    (void)data;//not used

    if(msg_len != sizeof(msg_send))
    {
        printk(KERN_ERR "Incorrect length of message, recieved by test service.\n");
        return;
    }
    if(memcmp(msg, msg_send, msg_len))
    {
        printk(KERN_ERR "Incorrect content of message, recieved by test service.\n");
        return;

    }
    sc_send(interaction, msg_reply, sizeof(msg_reply));
}



static __init int test_init(void)
{
    if(sc_register_callback_for_type(TEST_IT, test_service, NULL, NULL))
    {
        printk(KERN_ERR "Failed to register test service");
        return -1;
    }
	return 0;
}

static __exit void test_exit(void)
{
	sc_unregister_callback_for_type(TEST_IT, 1);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tsyvarev Andrey");

module_init(test_init);
module_exit(test_exit);