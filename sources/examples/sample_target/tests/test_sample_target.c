#include <stdio.h>

//#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>

#define FILENAME "/dev/cfake0"

char write_buffer[135];
void fill_write_buffer()
{
    int i;
    for(i = 0; i < sizeof(write_buffer); i++)
        write_buffer[i] = (char)(i * 2 + 3);
}
int main()
{
    int result = 0;
    char buffer[sizeof(write_buffer) + 110];
    
    int fd = open(FILENAME, O_RDWR);
    if(fd == 0)
    {
        printf("Cannot open file %s.\n", FILENAME);
        return 1;
    }
    if(write(fd, write_buffer, sizeof(write_buffer)) != sizeof(write_buffer))
    {
        printf("Cannot write all bytes from write_buffer\n");
        result = 1;
        goto out;
    }
    if(lseek(fd, SEEK_SET, 0) != 0)
    {
        printf("Cannot positioning offset to the start of the file\n");
        result = 1;
        goto out;
    }
    if(read(fd, buffer, sizeof(buffer)) < sizeof(write_buffer))
    {
        printf("Incorrect number of reading bytes.\n");
        result = 1;
        goto out;
    }
    if(memcmp(buffer, write_buffer, sizeof(write_buffer)) != 0)
    {
        printf("Bytes, that was read from file, differ from that was written into file previously.\n");
        result = 1;
        goto out;
    }
out:
    close(fd);
    return result;
}