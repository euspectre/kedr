# posix_acl_from_xattr() may or may not accept 'struct user_namespace *' as
# the first argument depending on kernel version and configuration. 
check_xattr_user_ns()

if (POSIX_ACL_XATTR_HAS_USER_NS)
	set(data_suffix "userns")
else ()
	set(data_suffix "plain")
endif (POSIX_ACL_XATTR_HAS_USER_NS)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/posix_acl_from_xattr.data.${data_suffix}"
	"${CMAKE_CURRENT_BINARY_DIR}/posix_acl_from_xattr.data"
	COPYONLY
)

func_db_configure_functions (
	"kstrdup"
	"kstrndup"
	"kmemdup"
	
	"add_to_page_cache_lru"
	"add_to_page_cache_locked"
	"__page_cache_alloc"

	"posix_acl_alloc"
	"posix_acl_clone"
	"posix_acl_from_mode"

	# TODO
	# Since kernel 3.14-rc1, semantics of posix_acl_create() and
	# posix_acl_chmod() has changed. See these commits in the mainline:
	# 37bc15392a2363ca822b2c2828e0ccafbea32f75
	# 5bf3258fd2acd8515450ab8efcd97c9d3b69f7f9
	# The old functions have been renamed with '__' prepended to their
	# names.
	#
	# When this API stablilizes a bit, we may check the signatures of
	# these functions and handle them appropriately. For now, the
	# payload modules may just exclude these from handling if the
	# '__*' functions exist.
	"posix_acl_create"
	"posix_acl_chmod"
	"__posix_acl_create"
	"__posix_acl_chmod"
	
	"match_strdup"
	
	"call_rcu"
	"call_rcu_sched"
	"kfree_call_rcu"

	GENERATED
	"posix_acl_from_xattr"
)

