#include <string.h> /* memcmp */

#include <kedr/syscall_connector/syscall_connector.h>
#include "../common.h"
#include <unistd.h> /* getpid() */
#include <stdio.h> /* printf() */

sc_interaction_id test_it = -1;

int main()
{
    char buf[sizeof(msg_reply)] = "";
    
    if(sc_library_try_use(TEST_LIBRARY_NAME, getpid(), &test_it, sizeof(test_it)))
    {
        printf("Cannot use named library service.\n");
        return 1;
    }
    
    sc_interaction* interaction = sc_interaction_create(test_it, getpid());
    if(!interaction)
    {
        printf("Cannot create interaction.\n");
        sc_library_unuse(TEST_LIBRARY_NAME, getpid());
        return 1;
    }
    if(sc_send(interaction, msg_send, sizeof(msg_send)) != sizeof(msg_send))
    {
        printf("Error occures while sending message.\n");
        sc_interaction_destroy(interaction);
        sc_library_unuse(TEST_LIBRARY_NAME, getpid());
        return 1;
    }
    if(sc_recv(interaction, buf, sizeof(buf)) != sizeof(buf))
    {
        printf("Error occures while recieving message.\n");
        sc_interaction_destroy(interaction);
        sc_library_unuse(TEST_LIBRARY_NAME, getpid());
        return 1;
    }
    sc_interaction_destroy(interaction);
    if(memcmp(buf, msg_reply, sizeof(buf)))
    {
        printf("Incorrect content of message recieved.\n");
        sc_interaction_destroy(interaction);
        sc_library_unuse(TEST_LIBRARY_NAME, getpid());
        return 1;
    }
    sc_library_unuse(TEST_LIBRARY_NAME, getpid());
    return 0;
}
