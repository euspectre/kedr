[group]
function.name = posix_acl_from_xattr
trigger.code =>>
	struct posix_acl *acl;
	acl = posix_acl_from_mode(S_IRWXU, GFP_KERNEL);
	if (acl) {
		struct posix_acl *acl_new;
		void *value;
		int err; 
		size_t size = posix_acl_xattr_size(acl->a_count);
		
		value = kmalloc(size, GFP_KERNEL);
		if (value == NULL)
			goto out;

		err = posix_acl_to_xattr(acl, value, size);
		if (err <= 0)
			goto out_kfree;
		
		acl_new = posix_acl_from_xattr(value, size);
		if (!IS_ERR(acl_new))
			posix_acl_release(acl_new);
out_kfree:		
		kfree(value);
out:		
		posix_acl_release(acl);
	}
<<
