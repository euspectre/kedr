/* trigger_module.h */

#ifndef TRIGGER_MODULE_H
#define TRIGGER_MODULE_H

#include <linux/ioctl.h> /* needed for the _IOW macro */


/* ====================================================== */
/* ioctl-related stuff */

/* A 'magic' number to distinguish the IOCTL codes for our driver */
#define TRIGGER_IOCTL_MAGIC  0xC2
#define TRIGGER_IOCTL_CODE(nr) _IOWR(TRIGGER_IOCTL_MAGIC, nr, int)

#endif /* TRIGGER_MODULE_H */
