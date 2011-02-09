

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL"); 

static void do_something(void);
static void __user *user_area = NULL;

static int __init
probe_init_module(void)
{
	do_something();
	return 0;
}

static void __exit
probe_exit_module(void)
{
	return;
}

module_init(probe_init_module);
module_exit(probe_exit_module);

static void
do_something(void)
{

