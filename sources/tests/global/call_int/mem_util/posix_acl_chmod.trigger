[group]
function.name = posix_acl_chmod
trigger.code =>>
	struct posix_acl *acl;
	acl = posix_acl_from_mode(S_IRWXU, GFP_KERNEL);
	if (!IS_ERR(acl)) {
		int err;
		umode_t mode = S_IRWXU;
		err = posix_acl_chmod(&acl, GFP_KERNEL, mode);
		if (err >= 0 && acl != NULL)
			posix_acl_release(acl);
	}
<<
