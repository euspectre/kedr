#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/refcount.h>
#include <linux/fs.h>
#include <linux/posix_acl.h>

MODULE_LICENSE("GPL");

/* This code will never be run, only compiled. */

struct posix_acl acl;

static int __init
my_init(void)
{
	memset(&acl, 0, sizeof(acl));
	refcount_inc(&acl.a_refcount);
	pr_info("acl is %p\n", &acl);
	return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
