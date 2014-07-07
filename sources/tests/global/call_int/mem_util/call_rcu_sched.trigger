[group]
header =>>
static void 
kedr_trigger_cb_call_rcu_sched(struct rcu_head *rcu_head)
{
	struct kedr_foo *kedr_foo; 
	
	BUG_ON(rcu_head == NULL);
	
	kedr_foo = container_of(rcu_head, struct kedr_foo, rcu_head);
	kfree(kedr_foo);
}
<<

function.name = call_rcu_sched
trigger.code =>>
	struct kedr_foo *kedr_foo;
	kedr_foo = kmalloc(sizeof(struct kedr_foo), GFP_KERNEL);
	if (kedr_foo) {
		call_rcu_sched(&kedr_foo->rcu_head, 
                        kedr_trigger_cb_call_rcu_sched);
	
		/* The barrier is here to make sure that any pending RCU 
		 * callbacks have finished before the module is unloaded. */
		rcu_barrier();
	}
<<
