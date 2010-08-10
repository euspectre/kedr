#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <fcntl.h>
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
        printf("Usage:\n\t%s function_to_trigger\n", argv[0]);
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
    return EXIT_FAILURE;
}

int trigger_function(int nr)
{
    int arg = 10; //parameter of ioctl
    int result;

    int fd;

    errno = 0;
    fd = open(FILENAME, O_RDWR);
    if(fd < 0)
    {
        printf("Unable to open \"%s\", errno is %d (\"%s\")\n", 
            FILENAME, errno, strerror(errno));
        return EXIT_FAILURE;
    }

    errno = 0;
    result = ioctl(fd, TRIGGER_IOCTL_CODE(nr), &arg);
    if (result == -1)
    {
        printf("ioctl() failed, errno is %d (\"%s\")\n", 
            errno, strerror(errno));
    }
    else
    {
        result = 0;
    }

    close(fd);
    return result;
}

