#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "trigger_common.h"

#define FILENAME "/dev/kedr_trigger_device"

/*
 * Index of function in the array correspond to 'nr' part of ioctl code,
 * which should be send to the kernel module for trigger function
 */
const char* function_names[]=
{
    "first_item",
    <$array_item : join(,\n)$>
};

int trigger_function(int nr);

int main(int argc, char** argv)
{
    const char* function;
    int nr;
    if(argc != 2)
    {
        printf("Usage:\n\n\t%s function_for_trigger\n", argv[0]);
        return 1;
    }
    function = argv[1];
    for(nr = 1; nr < sizeof(function_names)/sizeof(function_names[0]); nr++)
    {
        if(strcmp(function_names[nr], function) == 0)
        {
            return trigger_function(nr);
        }
    }
    printf("There is no trigger for function %s.\n", function);
    return 1;
}

int trigger_function(int nr)
{
    int arg = 10;//parameter of ioctl
    int result;
    int fd = open(FILENAME, "r");
    if(fd == 0)
    {
        printf("Cannot open file '%s'", FILENAME);
        return 1;
    }
    result = ioctl(fd, TRIGGER_IOCTL_CODE(nr), &arg);
    close(fd);
    return result;

}