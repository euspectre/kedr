# Unfortunately, we do not currently provide call interception tests 
# for the following functions: 
# 	 add_to_page_cache_lru
#	 add_to_page_cache_locked
#	 __page_cache_alloc
#	posix_acl_from_xattr
# The main problem here is how to create a reasonably simple code to use
# these functions in the correct environment and how to properly clean up
# after that.
#
# As far as posix_acl_from_xattr() is concerned, it is currently not clear
# how to obtain appropriate struct user_namespace instance needed for this
# function, for example, in kernel 3.7.

kedr_test_add_ci_test("mem_util"
	# Functions:
	"kstrdup"
	"kstrndup"
	"kmemdup"
	
#	"add_to_page_cache_lru"
#	"add_to_page_cache_locked"
#	"__page_cache_alloc"

	"posix_acl_alloc"
	"posix_acl_clone"
	"posix_acl_from_mode"

	"posix_acl_create"
	"posix_acl_chmod"
#	"posix_acl_from_xattr"

	"match_strdup"
	
	"call_rcu"
	"call_rcu_sched"
	"kfree_call_rcu"
)
