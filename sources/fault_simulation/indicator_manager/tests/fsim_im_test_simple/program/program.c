#include <kedr/fault_simulation/fsim_indicator_manager.h>

#include <../common.h>

#include <stdio.h> /*printf */

int main()
{
    if(kedr_fsim_set_indicator(point_name, indicator_name,
        NULL, 0))
    {
        printf("Cannot set indicator for point.\n");
        return 1;
    }
    return 0;
    
}