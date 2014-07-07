[group]
function.name = posix_acl_alloc
trigger.code =>>
	struct posix_acl *acl;
	acl = posix_acl_alloc(1, GFP_KERNEL);
	if (acl)
		posix_acl_release(acl);
<<
