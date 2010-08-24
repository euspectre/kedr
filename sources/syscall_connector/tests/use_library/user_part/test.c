#include <string.h> /* memcmp */

#include <kedr/syscall_connector/syscall_connector.h>
#include "../common.h"
#include <unistd.h> /* getpid() */
#include <stdio.h> /* printf() */

int main()
{
    char buf[sizeof(msg_reply)] = "";
    sc_interaction* interaction = sc_interaction_create(TEST_IT, getpid());
    if(!interaction)
    {
        printf("Cannot create interaction.\n");
        return 1;
    }
    if(sc_send(interaction, msg_send, sizeof(msg_send)) != sizeof(msg_send))
    {
        printf("Error occures while sending message.\n");
        sc_interaction_destroy(interaction);
        return 1;
    }
    if(sc_recv(interaction, buf, sizeof(buf)) != sizeof(buf))
    {
        printf("Error occures while recieving message.\n");
        sc_interaction_destroy(interaction);
        return 1;
    }
    sc_interaction_destroy(interaction);
    if(memcmp(buf, msg_reply, sizeof(buf)))
    {
        printf("Incorrect content of message recieved.\n");
        sc_interaction_destroy(interaction);
        return 1;
    }
    return 0;
}
