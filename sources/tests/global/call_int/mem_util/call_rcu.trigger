[group]
header =>>
static void 
kedr_trigger_cb_call_rcu(struct rcu_head *rcu_head)
{
	struct kedr_foo *kedr_foo; 
	
	BUG_ON(rcu_head == NULL);
	
	kedr_foo = container_of(rcu_head, struct kedr_foo, rcu_head);
	kfree(kedr_foo);
}
<<

function.name = call_rcu
trigger.code =>>
	struct kedr_foo *kedr_foo;
	kedr_foo = kmalloc(sizeof(struct kedr_foo), GFP_KERNEL);
	if (kedr_foo) {
		call_rcu(&kedr_foo->rcu_head, kedr_trigger_cb_call_rcu);
	
		/* The barrier is here to make sure that any pending RCU 
		 * callbacks have finished before the module is unloaded. */
		rcu_barrier();
	}
<<
