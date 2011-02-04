//user_area - external pointer to user data, type 'void * __user'
void *p = memdup_user(user_area, 1);
if (!IS_ERR(p)) {
	kfree(p);
} else {
	printk(KERN_INFO 
"Failed to copy data from user space with memdup_user(), errno=%d.", 
		(int)PTR_ERR(p)); 
}