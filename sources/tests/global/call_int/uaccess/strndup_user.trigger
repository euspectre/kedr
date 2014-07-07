[group]
function.name = strndup_user

trigger.copy_from_user = yes
trigger.copy_from_user.buffer_size = 1

trigger.code =>>
	//user_area - external pointer to user data, type 'void * __user'
	char *p = strndup_user((const char __user *)user_area, 1);
	if (!IS_ERR(p)) {
		kfree(p);
	} else {
		printk(KERN_INFO 
	"Failed to copy data from user space with strndup_user(), errno=%d.", 
			(int)PTR_ERR(p)); 
	}
<<
