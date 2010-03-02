#include <linux/kprobes.h>
#include <linux/module.h>
struct kprobe kp = {};

MODULE_LICENSE("GPL");

void handler_post(struct kprobe *k, struct pt_regs *regs, unsigned long flags)
{
	printk(KERN_INFO "post_handler: addr=0x%p, eflags=0x%lx\n",
		k->addr, regs->flags);
	return;
}

int __init init_kprobe_test(void)
{
	kp.post_handler = handler_post;
	kp.symbol_name = "load_module";

	if(register_kprobe(&kp))
	{
		printk(KERN_INFO "Cannot register kprobe.\n");
		return 1;
	}
	
	printk(KERN_INFO "kprobe module was loaded.\n");
	return 0;
}

void __exit exit_kprobe_test(void)
{
	unregister_kprobe(&kp);
	printk(KERN_INFO "kprobe module was unloaded.\n");
}

module_init(init_kprobe_test);
module_exit(exit_kprobe_test);