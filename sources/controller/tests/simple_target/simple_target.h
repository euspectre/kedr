/* simple_target.h */

#ifndef SIMPLE_TARGET_H_1727_INCLUDED
#define SIMPLE_TARGET_H_1727_INCLUDED

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#ifndef KEDR_TEST_MAJOR
#define KEDR_TEST_MAJOR 0   /* dynamic major by default */
#endif

/* Number of devices to create (default: kedr_test0 and kedr_test1) */
#ifndef KEDR_TEST_NDEVICES
#define KEDR_TEST_NDEVICES 2    
#endif

/* Size of a circular buffer used for data storage */
#ifndef KEDR_TEST_BUFFER_SIZE
#define KEDR_TEST_BUFFER_SIZE 4000
#endif

/* Maxumum length of a block that can be read or written in one operation */
#ifndef KEDR_TEST_BLOCK_SIZE
#define KEDR_TEST_BLOCK_SIZE 512
#endif

/* The structure to represent 'kedr_test' devices */
struct kedr_test_dev {
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
extern int kedr_test_major;     
extern int kedr_test_ndevices;
extern unsigned long kedr_test_buffer_size;
extern unsigned long kedr_test_block_size;

/* ====================================================== */
/* ioctl-related stuff */

/* A 'magic' number to distinguish the IOCTL codes for our driver */
#define KEDR_TEST_IOCTL_MAGIC  0xC3 

/* IOCTL codes */
/* Reset the 'device' by filling the memory block with zeros */
#define KEDR_TEST_IOCTL_RESET _IO(KEDR_TEST_IOCTL_MAGIC, 0)
	
/* Fill the memory buffer with the specified character, 
the character converted to int being pointed to by the argument*/
#define KEDR_TEST_IOCTL_FILL  _IOW(KEDR_TEST_IOCTL_MAGIC, 1, int)

/* Load 'firmware' to the 'device': a predefined string "Hello, hacker!" */
#define KEDR_TEST_IOCTL_LFIRM _IO(KEDR_TEST_IOCTL_MAGIC, 2)

/* Read buffer size */
#define KEDR_TEST_IOCTL_RBUFSIZE _IOR(KEDR_TEST_IOCTL_MAGIC, 3, int /* ??? why int? */)

/* Set block size and return its previous value */
#define KEDR_TEST_IOCTL_SBLKSIZE _IOWR(KEDR_TEST_IOCTL_MAGIC, 4, int /* ??? why int? */)

/* The number of IOCTL codes */
#define KEDR_TEST_IOCTL_NCODES 5

#endif /* SIMPLE_TARGET_H_1727_INCLUDED */
