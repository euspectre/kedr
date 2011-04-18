[group]
function.name = memdup_user

trigger.copy_from_user = yes
trigger.copy_from_user.buffer_size = 1

trigger.code =>>
	//user_area - external pointer to user data, type 'void * __user'
	void *p = memdup_user(user_area, 1);
	if (!IS_ERR(p)) {
		kfree(p);
	} else {
		printk(KERN_INFO 
	"Failed to copy data from user space with memdup_user(), errno=%d.", 
			(int)PTR_ERR(p)); 
	}
<<
