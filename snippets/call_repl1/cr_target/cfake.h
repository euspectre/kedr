/* cfake.h */

#ifndef CFAKE_H_1727_INCLUDED
#define CFAKE_H_1727_INCLUDED

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#ifndef CFAKE_MAJOR
#define CFAKE_MAJOR 0   /* dynamic major by default */
#endif

/* Number of devices to create (default: cfake0 and cfake1) */
#ifndef CFAKE_NDEVICES
#define CFAKE_NDEVICES 2    
#endif

/* Size of a circular buffer used for data storage */
#ifndef CFAKE_BUFFER_SIZE
#define CFAKE_BUFFER_SIZE 4000
#endif

/* Maxumum length of a block that can be read or written in one operation */
#ifndef CFAKE_BLOCK_SIZE
#define CFAKE_BLOCK_SIZE 512
#endif

/* The structure to represent 'cfake' devices */
struct cfake_dev {
	/* Data buffer (linear for now) */
	unsigned char *data;
	
	/* Size of the data buffer */
	unsigned long buffer_size; 
	
	/* Maximum number of bytes that can be read or written in one call */
	unsigned long block_size;  
	
	/* Mutual exclusion semaphore */
	struct semaphore sem; 
	
	/* 1 if the device was successfuly added, 0 otherwise */
	int dev_added;
	
	/* Char device structure */
	struct cdev cdevice;     
};

/*
 * The configurable parameters
 */
extern int cfake_major;     
extern int cfake_ndevices;
extern unsigned long cfake_buffer_size;
extern unsigned long cfake_block_size;

/* ====================================================== */
/* ioctl-related stuff */

/* A 'magic' number to distinguish the IOCTL codes for our driver */
#define CFAKE_IOCTL_MAGIC  0xC2 

/* IOCTL codes */
/* Reset the 'device' by filling the memory block with zeros */
#define CFAKE_IOCTL_RESET _IO(CFAKE_IOCTL_MAGIC, 0)
	
/* Fill the memory buffer with the specified character, 
the character converted to int being pointed to by the argument*/
#define CFAKE_IOCTL_FILL  _IOW(CFAKE_IOCTL_MAGIC, 1, int)

/* Load 'firmware' to the 'device': a predefined string "Hello, hacker!" */
#define CFAKE_IOCTL_LFIRM _IO(CFAKE_IOCTL_MAGIC, 2)

/* Read buffer size */
#define CFAKE_IOCTL_RBUFSIZE _IOR(CFAKE_IOCTL_MAGIC, 3, int /* ??? why int? */)

/* Set block size and return its previous value */
#define CFAKE_IOCTL_SBLKSIZE _IOWR(CFAKE_IOCTL_MAGIC, 4, int /* ??? why int? */)

/* The number of IOCTL codes */
#define CFAKE_IOCTL_NCODES 5

#endif /* CFAKE_H_1727_INCLUDED */
