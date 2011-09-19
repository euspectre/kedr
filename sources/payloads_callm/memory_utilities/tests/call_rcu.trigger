[group]
function.name = call_rcu
trigger.code =>>
	struct kedr_foo *kedr_foo;
	kedr_foo = kmalloc(sizeof(struct kedr_foo), GFP_KERNEL);
	if (kedr_foo) {
		call_rcu(&kedr_foo->rcu_head, kedr_trigger_rcu_callback);
	
		/* The barrier is here to make sure that any pending RCU 
		 * callbacks have finished before the module is unloaded. */
		rcu_barrier();
	}
<<
