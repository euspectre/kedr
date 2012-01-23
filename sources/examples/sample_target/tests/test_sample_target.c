/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Copyright (C) 2010-2011, Institute for System Programming 
 *                          of the Russian Academy of Sciences (ISPRAS)
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FILENAME "/dev/cfake1"

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
        printf("Failed to open file %s.\n", FILENAME);
        return 1;
    }
    if(write(fd, write_buffer, sizeof(write_buffer)) != sizeof(write_buffer))
    {
        printf("Failed to write all bytes from write_buffer\n");
        result = 1;
        goto out;
    }
    if(lseek(fd, SEEK_SET, 0) != 0)
    {
        printf("Failed to position file offset to the start of the file\n");
        result = 1;
        goto out;
    }
    if(read(fd, buffer, sizeof(buffer)) < sizeof(write_buffer))
    {
        printf("Incorrect number of read bytes.\n");
        result = 1;
        goto out;
    }
    if(memcmp(buffer, write_buffer, sizeof(write_buffer)) != 0)
    {
        printf( "The bytes read from the file differ from what was "
				"previously written to it.\n");
        result = 1;
        goto out;
    }
out:
    close(fd);
    return result;
}