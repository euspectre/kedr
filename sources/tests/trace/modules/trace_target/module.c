/* 
 * 'Target' module for tracing.
 * Generate trace messages via reading/writing file in debugfs.
 * 
 * Writing to the file generates message, dependent from the
 * string written.
 * 
 * Reading from the file allows to generate pair of messages under one lock,
 * so this messages should come paired in the trace.
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h> /*printk*/

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <linux/debugfs.h> /*debugfs_**/
#include <linux/fs.h> /*file operations*/
#include <linux/delay.h> /*msleep*/

#include <kedr/trace/trace.h>
#include <kedr_trace_test.h>

#include <linux/slab.h>
#include <linux/uaccess.h>

DEFINE_SPINLOCK(lock);

static int tt_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}
static ssize_t tt_read(struct file *filp,
    char __user* buf, size_t count, loff_t *f_pos)
{
	//read cannot occure in interrupt, so use simple spin_lock
	spin_lock(&lock);
	kedr_trace_test_msg("block_in");
	kedr_trace_test_msg("block_out");
	spin_unlock(&lock);
	//some delay
	msleep(100);
	
	return count;
}

static ssize_t tt_write(struct file *filp,
    const char __user* buf, size_t count, loff_t *f_pos)
{
	size_t len = count;
	char* str = kmalloc(len, GFP_KERNEL);
	if(!str) return -ENOMEM;
	
	if(copy_from_user(str, buf, len))
	{
		kfree(str);
		return -EFAULT;
	}
	
	if(len && str[len - 1] == '\n')	len--;
	
	kedr_trace_test_msg_len(str, len);
	
	kfree(str);
	
	return count;
}


static struct file_operations tt_ops =
{
	.owner = THIS_MODULE,
	.open = tt_open,
	.read = tt_read,
	.write = tt_write,
};

static struct dentry* control_file = NULL;

static int __init
tt_init(void)
{
	control_file = debugfs_create_file("kedr_trace_test_control", S_IRUSR,
		NULL, NULL,	&tt_ops);
	
	if(!control_file)
	{
		pr_err("Cannot create control file.");
		return -EINVAL;
	}

	return 0;
}
static void
tt_exit(void)
{
	debugfs_remove(control_file);
}

module_init(tt_init);
module_exit(tt_exit);
