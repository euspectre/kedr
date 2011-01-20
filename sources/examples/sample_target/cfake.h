/* cfake.h */

#ifndef CFAKE_H_1727_INCLUDED
#define CFAKE_H_1727_INCLUDED

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
	
	/* Mutual exclusion */
	struct mutex cfake_mutex; 
	
	/* 1 if the device was successfuly added, 0 otherwise */
	int dev_added;
	
	/* Char device structure */
	struct cdev cdevice;     
};

/*
 * Configurable parameters
 */
extern int cfake_major;     
extern int cfake_ndevices;
extern unsigned long cfake_buffer_size;
extern unsigned long cfake_block_size;

#endif /* CFAKE_H_1727_INCLUDED */
