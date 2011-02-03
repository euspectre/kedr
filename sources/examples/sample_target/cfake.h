/* cfake.h */

#ifndef CFAKE_H_1727_INCLUDED
#define CFAKE_H_1727_INCLUDED

/* Number of devices to create (default: cfake0 and cfake1) */
#ifndef CFAKE_NDEVICES
#define CFAKE_NDEVICES 2    
#endif

/* Size of a buffer used for data storage */
#ifndef CFAKE_BUFFER_SIZE
#define CFAKE_BUFFER_SIZE 4000
#endif

/* Maxumum length of a block that can be read or written in one operation */
#ifndef CFAKE_BLOCK_SIZE
#define CFAKE_BLOCK_SIZE 512
#endif

/* The structure to represent 'cfake' devices. 
 *  data - data buffer;
 *  buffer_size - size of the data buffer;
 *  block_size - maximum number of bytes that can be read or written 
 *    in one call;
 *  cfake_mutex - a mutex to protect the fields of this structure;
 *  cdev - ñharacter device structure.
 */
struct cfake_dev {
	unsigned char *data;
	unsigned long buffer_size; 
	unsigned long block_size;  
	struct mutex cfake_mutex; 
	struct cdev cdev;
};
#endif /* CFAKE_H_1727_INCLUDED */
