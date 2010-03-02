#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <errno.h>

/* ================================================================= */
/* ioctl-related stuff */

/* A 'magic' number to distinguish the IOCTL codes for our driver */
#define ICPT_TARGET_IOCTL_MAGIC  0xC2 

/* IOCTL codes */
/* Go do some work */
#define ICPT_TARGET_IOCTL_GO _IO(ICPT_TARGET_IOCTL_MAGIC, 0)
/* ================================================================= */

static const char device[] = "/dev/icpt_target";

/*
argc > 1 => use ioctl().
*/

int
main(int argc, char* argv[])
{
	printf("Calling open() for %s\n", device);
	int fd = open(device, O_RDONLY);
	if (fd < 0)
	{
		fprintf(stderr, "[Error] open(): %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	
	if (argc > 1)
	{
		printf("Calling ioctl() for %s\n", device);
		if (ioctl(fd, ICPT_TARGET_IOCTL_GO, NULL) == -1)	
		{
			fprintf(stderr, "[Error] ioctl() at line %d: %s\n", 
				__LINE__, strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}
	}
	
	printf("Calling close() for %s\n", device);
	close(fd);
	return EXIT_SUCCESS;
}

