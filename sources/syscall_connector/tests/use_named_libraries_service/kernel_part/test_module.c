#include <kedr/syscall_connector/syscall_connector.h>
#include "../common.h"

#include <linux/module.h>
    
sc_interaction_id test_it = -1;

static void test_service(const sc_interaction* interaction, const void* msg, size_t msg_len, void* data)
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

static int test_try_use(void)
{
    return !try_module_get(THIS_MODULE);
}
static void test_unuse(void)
{
    module_put(THIS_MODULE);
}

static __init int test_init(void)
{
    test_it = sc_register_callback_for_unused_type(test_service, NULL, NULL);
    if(test_it == -1)
    {
        printk(KERN_ERR "Failed to register test service.\n");
        return -1;
    }
    if(sc_library_register(TEST_LIBRARY_NAME, test_try_use, test_unuse,
        &test_it, sizeof(test_it)))
    {
        sc_unregister_callback_for_type(test_it, 1);
        printk(KERN_ERR "Failed to register library for test.\n");
        return -1;

    }
	return 0;
}

static __exit void test_exit(void)
{
	sc_library_unregister(TEST_LIBRARY_NAME);
	sc_unregister_callback_for_type(test_it, 1);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tsyvarev Andrey");

module_init(test_init);
module_exit(test_exit);