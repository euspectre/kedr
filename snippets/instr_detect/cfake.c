#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/cdev.h>

#include <asm/uaccess.h>	/* copy_*_user */

#include <asm/insn.h>		/* instruction decoder machinery */

#include "cfake.h"

MODULE_AUTHOR("Eugene");

/* Because this module uses the instruction decoder which is distributed
 * under GPL, I have no choice but to distribute this module under GPL too.
 * */
MODULE_LICENSE("GPL");

/* parameters */
int cfake_major = CFAKE_MAJOR;
int cfake_minor = 0;
int cfake_ndevices = CFAKE_NDEVICES;
unsigned long cfake_buffer_size = CFAKE_BUFFER_SIZE;
unsigned long cfake_block_size = CFAKE_BLOCK_SIZE;

module_param(cfake_major, int, S_IRUGO);
module_param(cfake_minor, int, S_IRUGO);
module_param(cfake_ndevices, int, S_IRUGO);
module_param(cfake_buffer_size, ulong, S_IRUGO);
module_param(cfake_block_size, ulong, S_IRUGO);

/* ================================================================ */
/* Ctors and dtors - just to check if they are called and in what order */
/* 
__attribute__((constructor))
void
my_ctor2(void)
{
	printk(KERN_NOTICE "[ctor_sample] my_ctor2() called\n");
	return;
}
 
__attribute__((constructor))
void
my_ctor1(void)
{
	void* p;
	printk(KERN_NOTICE "[ctor_sample] my_ctor1() called\n");
	p = __kmalloc(1024, GFP_KERNEL);
	kfree(p);
	p = __kmalloc(1024, GFP_KERNEL);
	kfree(p);
	return;
}
 
__attribute__((destructor))
void
my_dtor1(void)
{
	void* p;
	printk(KERN_NOTICE "[ctor_sample] my_dtor1() called\n");
	p = __kmalloc(1024, GFP_KERNEL);
	kfree(p);
	p = __kmalloc(1024, GFP_KERNEL);
	kfree(p);
	p = __kmalloc(1024, GFP_KERNEL);
	kfree(p);
	p = __kmalloc(1024, GFP_KERNEL);
	kfree(p);
	return;
}
 
__attribute__((destructor))
void
my_dtor3(void)
{
	printk(KERN_NOTICE "[ctor_sample] my_dtor1() called\n");
	return;
}
 
__attribute__((destructor))
void
my_dtor2(void)
{
	printk(KERN_NOTICE "[ctor_sample] my_dtor1() called\n");
	return;
}
*/
/* ================================================================ */
/* Helpers */

/* Calculate the memory address being the operand of a given instruction 
 * (usually, 'call'). 
 *   'insn_addr' is the address of the instruction itself,
 *   'insn_len' is length of the instruction in bytes,
 *   'offset' is the offset of the destination address from the first byte
 *   past the instruction.
 * 
 * For x86_64 architecture, the offset value is sign-extended here first.
 * 
 * "Intel x86 Instruction Set Reference" states the following 
 * concerning 'call rel32':
 * 
 * "Call near, relative, displacement relative to next instruction.
 * 32-bit displacement sign extended to 64 bits in 64-bit mode."
 * */
#ifdef CONFIG_X86_64
#  define INSN_GET_CALL_ADDR(insn_addr, insn_len, offset) \
	(void*)((s64)(insn_addr) + (s64)(insn_len) + (s64)(s32)(offset))
#else /* CONFIG_X86_32 */
#  define INSN_GET_CALL_ADDR(insn_addr, insn_len, offset) \
	(void*)((u32)(insn_addr) + (u32)(insn_len) + (u32)(offset))
#endif

/* ================================================================ */
/* Main operations - declarations */

int cfake_open(struct inode *inode, struct file *filp);

int cfake_release(struct inode *inode, struct file *filp);

ssize_t cfake_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos);

ssize_t cfake_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos);

int cfake_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg);

loff_t cfake_llseek(struct file *filp, loff_t off, int whence);

/* ================================================================ */

static void test_insn_decoder(struct module* mod);

/* ================================================================ */

struct cfake_dev *cfake_devices;	/* created in cfake_init_module() */

struct file_operations cfake_fops = {
	.owner =    THIS_MODULE,
	.llseek =   cfake_llseek,
	.read =     cfake_read,
	.write =    cfake_write,
	.ioctl =    cfake_ioctl,
	.open =     cfake_open,
	.release =  cfake_release,
};

/* ================================================================ */
/* Set up the char_dev structure for the device. */
static void cfake_setup_cdevice(struct cfake_dev *dev, int index)
{
	int err;
	int devno = MKDEV(cfake_major, cfake_minor + index);
    
	cdev_init(&dev->cdevice, &cfake_fops);
	dev->cdevice.owner = THIS_MODULE;
	dev->cdevice.ops = &cfake_fops;
	
	err = cdev_add(&dev->cdevice, devno, 1);
	if (err)
	{
		printk(KERN_NOTICE "[CFake] Error %d while trying to add cfake%d",
			err, index);
	}
	else
	{
		dev->dev_added = 1;
	}
	return;
}

/* ================================================================ */
static void
cfake_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(cfake_major, cfake_minor);
	
	printk(KERN_ALERT "[CFake] Cleaning up\n");
	
	/* Get rid of our char dev entries */
	if (cfake_devices) {
		for (i = 0; i < cfake_ndevices; ++i) {
			kfree(cfake_devices[i].data);
			if (cfake_devices[i].dev_added)
			{
				cdev_del(&cfake_devices[i].cdevice);
			}
		}
		kfree(cfake_devices);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, cfake_ndevices);
	return;
}

static int __init
cfake_init_module(void)
{
	int result = 0;
	int i;
	dev_t dev = 0;
	struct module* this = THIS_MODULE;
	
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
	struct module* other = NULL;
	static const char other_mod_name[] = "vboxguest";
#endif
	
	printk(KERN_ALERT "[CFake] Initializing\n");
	
	if (cfake_ndevices <= 0)
	{
		printk(KERN_WARNING "[CFake] Invalid value of cfake_ndevices: %d\n", 
			cfake_ndevices);
		result = -EINVAL;
		return result;
	}
	
	/* Get a range of minor numbers to work with, asking for a dynamic
	major number unless directed otherwise at load time.
	*/
	if (cfake_major > 0) {
		dev = MKDEV(cfake_major, cfake_minor);
		result = register_chrdev_region(dev, cfake_ndevices, "cfake");
	} else {
		result = alloc_chrdev_region(&dev, cfake_minor, cfake_ndevices,
				"cfake");
		cfake_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "[CFake] can't get major number %d\n", cfake_major);
		return result;
	}
	
	/* Allocate the array of devices */
	cfake_devices = (struct cfake_dev*)kmalloc(
		cfake_ndevices * sizeof(struct cfake_dev), 
		GFP_KERNEL);
	if (cfake_devices == NULL) {
		result = -ENOMEM;
		goto fail;
	}
	memset(cfake_devices, 0, cfake_ndevices * sizeof(struct cfake_dev));
	
	/* Initialize each device. */
	for (i = 0; i < cfake_ndevices; ++i) {
		cfake_devices[i].buffer_size = cfake_buffer_size;
		cfake_devices[i].block_size = cfake_block_size;
		cfake_devices[i].dev_added = 0;
		
		/* memory is to be allocated in open() */
		cfake_devices[i].data = NULL; 
		
		init_MUTEX(&cfake_devices[i].sem);
		cfake_setup_cdevice(&cfake_devices[i], i);
	}
	
	/* Output some data for analysis */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
	/* Ensure noone else meddles with module structures now */
	if (mutex_lock_interruptible(&module_mutex) != 0) {
		result = -EINTR;
		goto fail;
	}
#endif
	
	/*printk( KERN_WARNING "[CFake] Ctors: "
	"#1 - 0x%p, #2 - 0x%p\n",
		(const void*)&my_ctor1,
		(const void*)&my_ctor2
	);
	
	printk( KERN_WARNING "[CFake] Dtors: "
	"#1 - 0x%p, #2 - 0x%p, #3 - 0x%p\n",
		(const void*)&my_dtor1,
		(const void*)&my_dtor2,
		(const void*)&my_dtor3
	);*/
	
	/*printk( KERN_WARNING "[CFake] Methods: "
	"init - 0x%p, cleanup - 0x%p, "
	"open - 0x%p, release - 0x%p, "
	"read - 0x%p, write - 0x%p, ioctl - 0x%p, llseek - 0x%p" "\n",
		(const void*)&cfake_init_module,
		(const void*)&cfake_cleanup_module,
		(const void*)&cfake_open,
		(const void*)&cfake_release,
		(const void*)&cfake_read,
		(const void*)&cfake_write,
		(const void*)&cfake_ioctl,
		(const void*)&cfake_llseek
	);*/
	
	/* First, the data from this module */
	printk(	KERN_WARNING "[CFake] module: \"%s\", "
"core: %p (size: %u, text_size: %u), init: %p (size: %u, text_size: %u)\n",
		this->name,
		this->module_core, 
		(unsigned int)this->core_size, 
		(unsigned int)this->core_text_size,
		this->module_init, 
		(unsigned int)this->init_size, 
		(unsigned int)this->init_text_size);
		
	test_insn_decoder(this);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
	/* Now find the other module and output appropriate data */
	other = find_module(other_mod_name);
	if (other == NULL) {
		printk(	KERN_WARNING "[CFake] not found module \"%s\"\n",
			other_mod_name);
	}
	else
	{
		printk(	KERN_WARNING "[CFake] module: \"%s\", "
"core: %p (size: %u, text_size: %u), init: %p (size: %u, text_size: %u)\n",
		other->name,
		other->module_core, 
		(unsigned int)other->core_size, 
		(unsigned int)other->core_text_size,
		other->module_init, 
		(unsigned int)other->init_size, 
		(unsigned int)other->init_text_size);
		
		test_insn_decoder(other);
	}
	
	mutex_unlock(&module_mutex);
#endif
	return 0; /* success */

fail:
	cfake_cleanup_module();
	return result;
}

module_init(cfake_init_module);
module_exit(cfake_cleanup_module);
/* ================================================================ */

int 
cfake_open(struct inode *inode, struct file *filp)
{
	unsigned int mj = imajor(inode);
	unsigned int mn = iminor(inode);
	
	struct cfake_dev *dev = NULL;
	
	printk(KERN_WARNING "[CFake] open() for MJ=%d and MN=%d\n", mj, mn);
	
	if (mj != cfake_major || mn < cfake_minor || 
		mn >= cfake_minor + cfake_ndevices)
	{
		printk(KERN_WARNING "[CFake] No device found with MJ=%d and MN=%d\n", 
			mj, mn);
		return -ENODEV; /* No such device */
	}
	
	/* store a pointer to struct cfake_dev here for other methods */
	dev = &cfake_devices[mn - cfake_minor];
	filp->private_data = dev; 
	
	if (inode->i_cdev != &dev->cdevice)
	{
		printk(KERN_WARNING "[CFake] open: internal error\n");
		return -ENODEV; /* No such device */
	}
	
	/* if opened the 1st time, allocate the buffer */
	if (dev->data == NULL)
	{
		dev->data = (unsigned char*)kmalloc(
			dev->buffer_size, 
			GFP_KERNEL);
		if (dev->data == NULL)
		{
			printk(KERN_WARNING "[CFake] open: out of memory\n");
			return -ENOMEM;
		}
		memset(dev->data, 0, dev->buffer_size);
	}
	return 0;
}

int 
cfake_release(struct inode *inode, struct file *filp)
{
	printk(KERN_WARNING "[CFake] release() for MJ=%d and MN=%d\n", 
		imajor(inode), iminor(inode));
	return 0;
}

ssize_t 
cfake_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	ssize_t retval = 0;
	
	printk(KERN_WARNING "[CFake] read() for MJ=%d and MN=%d\n", 
		imajor(filp->f_dentry->d_inode), iminor(filp->f_dentry->d_inode));

	if (down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	
	if (*f_pos >= dev->buffer_size) /* EOF */
	{
		goto out;
	}
	
	if (*f_pos + count > dev->buffer_size)
	{
		count = dev->buffer_size - *f_pos;
	}
	
	if (count > dev->block_size)
	{
		count = dev->block_size;
	}
	
	if (copy_to_user(buf, &(dev->data[*f_pos]), count) != 0)
	{
		retval = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	retval = count;
	
out:
  	up(&dev->sem);
	return retval;
}
                
ssize_t 
cfake_write(struct file *filp, const char __user *buf, size_t count, 
	loff_t *f_pos)
{
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	ssize_t retval = 0;
	
	printk(KERN_WARNING "[CFake] write() for MJ=%d and MN=%d\n", 
		imajor(filp->f_dentry->d_inode), iminor(filp->f_dentry->d_inode));
	
	if (down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	
	if (*f_pos >= dev->buffer_size) /* EOF */
	{
		goto out;
	}
	
	if (*f_pos + count > dev->buffer_size)
	{
		count = dev->buffer_size - *f_pos;
	}
	
	if (count > dev->block_size)
	{
		count = dev->block_size;
	}
	
	if (copy_from_user(&(dev->data[*f_pos]), buf, count) != 0)
	{
		retval = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	retval = count;
	
out:
  	up(&dev->sem);
	return retval;
}

int 
cfake_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	int fill_char;
	unsigned int block_size = 0;
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	
	printk(KERN_WARNING "[CFake] ioctl() for MJ=%d and MN=%d\n", 
		imajor(inode), iminor(inode));

	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != CFAKE_IOCTL_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > CFAKE_IOCTL_NCODES) return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}
	if (err) 
	{
		return -EFAULT;
	}
	
	/* Begin critical section */
	if (down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	
	switch(cmd) {
	case CFAKE_IOCTL_RESET:
		memset(dev->data, 0, dev->buffer_size);
		break;
	
	case CFAKE_IOCTL_FILL:
		retval = get_user(fill_char, (int __user *)arg);
		if (retval == 0) /* success */
		{
			memset(dev->data, fill_char, dev->buffer_size);
		}
		break;
	
	case CFAKE_IOCTL_LFIRM:
		/* Assume that only an administrator can load the 'firmware' */ 
		if (!capable(CAP_SYS_ADMIN))
		{
			retval = -EPERM;
			break;
		}
		
		memset(dev->data, 0, dev->buffer_size);
		strcpy((char*)(dev->data), "Hello, hacker!\n");
		break;
	
	case CFAKE_IOCTL_RBUFSIZE:
		retval = put_user(dev->buffer_size, (unsigned long __user *)arg);
		break;
	
	case CFAKE_IOCTL_SBLKSIZE:
		retval = get_user(block_size, (unsigned long __user *)arg);
		if (retval != 0) break;
		
		retval = put_user(dev->block_size, (unsigned long __user *)arg);
		if (retval != 0) break;
		
		dev->block_size = block_size;
		break;
	
	default:  /* redundant, as 'cmd' was checked against CFAKE_IOCTL_NCODES */
		retval = -ENOTTY;
	}
	
	/* End critical section */
	up(&dev->sem);
	return retval;
}

loff_t 
cfake_llseek(struct file *filp, loff_t off, int whence)
{
	struct cfake_dev *dev = (struct cfake_dev *)filp->private_data;
	loff_t newpos = 0;
	
	printk(KERN_WARNING "[CFake] read() for MJ=%d and MN=%d\n", 
		imajor(filp->f_dentry->d_inode), iminor(filp->f_dentry->d_inode));
	
	switch(whence) {
	  case 0: /* SEEK_SET */
		newpos = off;
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	  case 2: /* SEEK_END */
		newpos = dev->buffer_size + off;
		break;

	  default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0) return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}

/* ================================================================ */

/* Decode and process the instruction ('c_insn') at
 * the address 'kaddr'. 
 * Check if we get past the end of the buffer [kaddr, end_kaddr)
 * 
 * The function returns the length of the instruction in bytes. 
 * 0 is returned in case of failure.
 */
static unsigned int
do_process_insn(struct insn* c_insn, void* kaddr, void* end_kaddr)
{
	/* ptr to the 32-bit offset argument in the instruction */
	u32* offset = NULL; 
	
	/* address of the function being called */
	void* addr = NULL;
	
	/* For now, we are looking for the following 1-byte opcodes
	 * to handle them in a special way. 
	 * 'jmp' is here just in case.
	 */
	static unsigned char op[] = {
		0xe8, /* call <offset> */
		0xe9  /* jmp  <offset> */
	};
	int op_ind = -1; /* -1 <=> not found in op[] */
	
	/* Names and addresses of the functions of interest */
	static void* target_func_addrs[] = {
		(void*)&__kmalloc,
		(void*)&kzalloc,
		(void*)&kfree,
		(void*)&printk,
		(void*)&copy_from_user,
		(void*)&copy_to_user
	};
	
	static const char* target_func_names[] = {
		"__kmalloc",
		"kzalloc",
		"kfree",
		"printk",
		"copy_from_user",
		"copy_to_user"
	};
	
	int i;
	
	BUG_ON(	ARRAY_SIZE(target_func_addrs) != 
		ARRAY_SIZE(target_func_names));
	
	/* Decode the instruction and populate 'insn' structure */
	kernel_insn_init(c_insn, kaddr);
	insn_get_length(c_insn);
	
	if (c_insn->length == 0)
	{
		return 0;
	}
	
	if (kaddr + c_insn->length > end_kaddr)
	{
	/* Note: it is OK to stop at 'end_kaddr' but no further */
		printk( KERN_WARNING "[CFake] "
	"Instruction decoder stopped past the end of the section.\n");
	}
		
/* This call may be overkill as insn_get_length() probably has to decode 
 * the instruction completely.
 * Still, to operate safely, we need insn_get_opcode() before we can access
 * c_insn->opcode. 
 * The call is cheap anyway, no re-decoding is performed.
 */
	insn_get_opcode(c_insn); 
	for (i = 0; i < (int)ARRAY_SIZE(op); ++i)
	{
		if (c_insn->opcode.value == op[i])
		{
			op_ind = i;
			break;
		}
	}
	
	if (op_ind == -1)
	{
		return c_insn->length;
	}
	/* Now we have found one of the instructions of interest */
	
/* [NB] For some reason, the decoder stores the argument of 'call' and 'jmp'
 * as 'immediate' rather than 'displacement' (as Intel manuals name it).
 * May be it is a bug, may be it is not. 
 * Meanwhile, I'll call this value 'offset' to avoid confusion.
 */

	/* Call this before trying to access c_insn->immediate */
	insn_get_immediate(c_insn);
	
	if (c_insn->immediate.nbytes != 4)
	{
		printk( KERN_WARNING "[CFake] At 0x%p: "
	"opcode: 0x%x, "
	"immediate field is %u rather than 32 bits in size; "
	"insn.length = %u, insn.imm = %u, off_immed = %d\n",
			kaddr,
			(unsigned int)c_insn->opcode.value,
			8 * (unsigned int)c_insn->immediate.nbytes,
			c_insn->length,
			(unsigned int)c_insn->immediate.value,
			insn_offset_immediate(c_insn));
		
		return c_insn->length;
	}
	
	offset = (u32*)(kaddr + insn_offset_immediate(c_insn));
	addr = INSN_GET_CALL_ADDR(kaddr, c_insn->length, *offset);
	
	/* Check if one of the functions of interest is called */
	for (i = 0; i < (int)ARRAY_SIZE(target_func_addrs); ++i)
	{
		if (addr == target_func_addrs[i])
		{
			printk( KERN_WARNING "[CFake] At 0x%p: "
				"%s %s (addr=0x%p, displ=0x%x)\n",
				kaddr,
				(op_ind == 0 ? "call" : "jmp"),
				target_func_names[i],
				addr,
				(unsigned int)(*offset)
			);
			break;
		}
	}
	
	
	/* Uncomment this to print all found calls and jumps
	 * (this may result in a huge load of messages in the log).
	 */
	/*printk( KERN_WARNING "[CFake] At 0x%p: "
		"%s 0x%p (offset = 0x%x, stored = 0x%x)\n",
		kaddr,
		(op_ind == 0 ? "call" : "jmp"),
		INSN_GET_CALL_ADDR(kaddr, c_insn->length, *offset),
		(unsigned int)(*offset),
		(unsigned int)c_insn->immediate.value
	);
	*/
	
	return c_insn->length;
}

/* number of instructions to decode - for testing only */
#define CFAKE_INSN_NUM 500 

/* Process the instructions in [kbeg, kend) area.
 * The pointers are not const because the function can be used to 
 * instrument the instructions in the area.
 */
static void
do_process_area(void* kbeg, void* kend)
{
	struct insn c_insn; /* current instruction */
	unsigned int i;
	void* pos = NULL;
	
	/* TODO: provide assert-like wrapper */
	BUG_ON(kbeg == NULL);
	BUG_ON(kend == NULL);
	BUG_ON(kend < kbeg);
	
	/* TODO: in production code, check ALL instructons rather than
	 * just CFAKE_INSN_NUM. 
	 * This boundary is set only to avoid cluttering the log
	 */
	/*
	 * Uncomment 'i < CFAKE_INSN_NUM' to set the boundary.
	 * */
	pos = kbeg;
	for (i = 0; /*i < CFAKE_INSN_NUM*/; ++i)
	{
		unsigned int len;
		unsigned int k;

		len = do_process_insn(&c_insn, pos, kend);
		if (len == 0)	
		{
			printk( KERN_WARNING "[CFake] "
			"do_process_insn() returned 0\n");
			break;
		}

		if (pos + len > kend)
		{
			break;
		}
		
/* If the decoded instruction contains only zero bytes (this is the case,
 * for example, for one flavour of 'add'), skip to the first nonzero byte
 * after it. 
 * This is to avoid problems if there are two or more sections in the area
 * being analysed. Such situation is very unlikely - still have to find 
 * the example. Note that ctors and dtors seem to be placed to the same 
 * '.text' section as the ordinary functions ('.ctors' and '.dtors' sections
 * probably contain just the lists of their addresses or something similar).
 * 
 * As we are not interested in instrumenting 'add' or the like, we can skip 
 * to the next instruction that does not begin with 0 byte. If we are 
 * actually past the last instruction in the section, we get to the next 
 * section or to the end of the area this way which is what we want in this
 * case.
 */
		for (k = 0; k < len; ++k)
		{
			if (*((unsigned char*)pos + k) != 0) 
			{
				break;
			}
		}
		pos += len;
		
		if (k == len) 
		{
			/* all bytes are zero, skip the following 0s */
			while (pos < kend && *(unsigned char*)pos == 0)
			{
				++pos;
			}
		}

		if (pos >= kend)
		{
			break;
		}
	}
	
	return;
}

static void 
test_insn_decoder(struct module* mod)
{
	//struct insn c_insn; /* current instruction being decoded */
	/*static unsigned char test_bytes[] = 
		{0x00, 0x00, 0x00, 0x55, 
		 0x89, 0xe5, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0xe8,
		 0x00, 0x00, 0x00, 0x00};
	
	printk( KERN_WARNING "[CFake] Area: [0x%p, 0x%p) (%u byte(s))\n",
		&test_bytes[0], &test_bytes[0] + ARRAY_SIZE(test_bytes),
		ARRAY_SIZE(test_bytes));
	
	do_process_area(&test_bytes[0], 
		&test_bytes[0] + ARRAY_SIZE(test_bytes));*/
	
	BUG_ON(mod == NULL);
	BUG_ON(mod->module_core == NULL);
	
	if (mod->module_init != NULL)
	{
		printk( KERN_WARNING "[CFake] Module \"%s\", "
		"processing \"init\" area\n",
			mod->name);
			
		do_process_area(mod->module_init, 
			mod->module_init + mod->init_text_size);
	}

	printk( KERN_WARNING "[CFake] Module \"%s\", "
		"processing \"core\" area\n",
		mod->name);
		
	do_process_area(mod->module_core, 
		mod->module_core + mod->core_text_size);
	return;
}

/* ================================================================ */
