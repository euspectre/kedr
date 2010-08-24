#include <kedr/syscall_connector/syscall_connector.h>
#include <stdio.h>

int main()
{
    sc_interaction* interaction = sc_interaction_create(2,2);
    if(!interaction)
    {
        printf("Failed to create interaction\n");
        return 1;
    }
    sc_interaction_destroy(interaction);

    return 0;
}