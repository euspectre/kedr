[group]

function.name = kfree_call_rcu
trigger.code =>>
	struct kedr_foo *kedr_foo;
	kedr_foo = kmalloc(sizeof(struct kedr_foo), GFP_KERNEL);
	if (kedr_foo != NULL) {
		unsigned long offset = offsetof(struct kedr_foo, rcu_head);
		kfree_call_rcu(&kedr_foo->rcu_head, 
			(kedr_test_rcu_callback_type)offset);
	
		/* The barrier is here to make sure that any pending RCU 
		 * callbacks have finished before the module is unloaded.
		 * Not strictly necessary as we do not set any callbacks
		 * but makes no harm. */
		rcu_barrier();
	}
<<
