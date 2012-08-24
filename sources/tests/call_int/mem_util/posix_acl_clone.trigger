[group]
function.name = posix_acl_clone
trigger.code =>>
	struct posix_acl *acl;
	struct posix_acl *cloned_acl;
	acl = posix_acl_alloc(1, GFP_KERNEL);
	if (acl) {
		cloned_acl = posix_acl_clone(acl, GFP_KERNEL);
		posix_acl_release(acl);
		if (cloned_acl)
			posix_acl_release(cloned_acl);
	}
<<
