/* icpt_target.h */
/* The calls to __kmalloc() from icpt_target driver are to be intercepted
and will possibly be forced to fail by the 'watcher' driver.
 */

#ifndef ICPT_TARGET_H_1306_INCLUDED
#define ICPT_TARGET_H_1306_INCLUDED

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#ifndef ICPT_TARGET_MAJOR
#define ICPT_TARGET_MAJOR 0   /* dynamic major device number by default */
#endif

/* Size of each buffer used for data storage */
#ifndef ICPT_TARGET_BUFFER_SIZE
#define ICPT_TARGET_BUFFER_SIZE 1048
#endif

/* The structure to represent 'cfake' devices */
struct icpt_target_dev {
	/* Data buffer */
	unsigned char *data;
	
	/* Aux buffer */
	unsigned char *aux;
	
	/* Size of each buffer */
	unsigned long buffer_size; 
	
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
extern int icpt_target_major;     
extern unsigned long icpt_target_bsize;

/* ====================================================== */
/* ioctl-related stuff */

/* A 'magic' number to distinguish the IOCTL codes for our driver */
#define ICPT_TARGET_IOCTL_MAGIC  0xC2 

/* IOCTL codes */
/* Go do some work */
#define ICPT_TARGET_IOCTL_GO _IO(ICPT_TARGET_IOCTL_MAGIC, 0)

#endif /* ICPT_TARGET_H_1306_INCLUDED */
