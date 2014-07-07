[group]
function.name = posix_acl_from_mode
trigger.code =>>
	struct posix_acl *acl;
	acl = posix_acl_from_mode(S_IRWXU, GFP_KERNEL);
	if (!IS_ERR(acl))
		posix_acl_release(acl);
<<
