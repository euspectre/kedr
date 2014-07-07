#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h> /*printk*/

MODULE_AUTHOR("Tsyvarev");
MODULE_LICENSE("GPL");

#include <linux/debugfs.h> /*debugfs_**/
#include <linux/fs.h> /*file operations*/
#include <linux/delay.h> /*msleep*/

#include <kedr/trace/trace.h>

static int pp_block(char* dest, size_t size, const void* n)
{
	return snprintf(dest, size, "block_%s", *(const int*)n ? "in" : "out");
}

static void trace_block(int n)
{
	kedr_trace(pp_block, &n, sizeof(n));
}

DEFINE_SPINLOCK(lock);

static int ccom_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}
static ssize_t ccom_read(struct file *filp,
    char __user* buf, size_t count, loff_t *f_pos)
{
	//read cannot occure in interrupt, so use simple spin_lock
	spin_lock(&lock);
	trace_block(1);
	trace_block(0);
	spin_unlock(&lock);
	//some delay
	msleep(100);
	
	return count;
}

static struct file_operations ccom_ops =
{
	.owner = THIS_MODULE,
	.open = ccom_open,
	.read = ccom_read
};

static struct dentry* work_dir = NULL;
static struct dentry* control_file = NULL;

static int __init
ccom_init(void)
{
	work_dir = debugfs_create_dir("cross_cpu_ordering_module", NULL);
	if(work_dir == NULL)
	{
		pr_err("Cannot create working directory in debugfs");
		return -EINVAL;
	}
	control_file = debugfs_create_file("control", S_IRUSR,
		work_dir,
		NULL,
		&ccom_ops);
	
	if(control_file == NULL)
	{
		pr_err("Cannot create control file.");
		debugfs_remove(work_dir);
		return -EINVAL;
	}

	return 0;
}
static void
ccom_exit(void)
{
	debugfs_remove(control_file);
	debugfs_remove(work_dir);
	kedr_trace_pp_unregister();
}

module_init(ccom_init);
module_exit(ccom_exit);
