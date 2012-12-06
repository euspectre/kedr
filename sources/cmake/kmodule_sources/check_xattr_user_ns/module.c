#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/posix_acl_xattr.h>
#include <linux/posix_acl.h>

MODULE_LICENSE("GPL");

#define TEST_BUF_SIZE 256

static int __init
my_init(void)
{
	size_t size = TEST_BUF_SIZE;
	char value[TEST_BUF_SIZE];
	struct posix_acl *acl = NULL;
	struct user_namespace *user_ns = NULL;

	acl = posix_acl_from_xattr(user_ns, &value[0], size);

	pr_info("acl is %p\n", acl);
	return 0;
}

static void __exit
my_exit(void)
{
}

module_init(my_init);
module_exit(my_exit);
