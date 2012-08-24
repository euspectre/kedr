# Unfortunately, we do not currently provide call interception tests 
# for the following functions: 
# 	 add_to_page_cache_lru
#	 add_to_page_cache_locked
# The main problem here is how to create a reasonably simple code to use
# these functions in the correct environment and how to properly clean up
# after that. 

kedr_test_add_ci_test("mem_util"
	# Functions:
	"kstrdup"
	"kstrndup"
	"kmemdup"
	
#	"add_to_page_cache_lru"
#	"add_to_page_cache_locked"

	"posix_acl_alloc"
	"posix_acl_clone"
	"posix_acl_from_mode"

	"match_strdup"
	
	"call_rcu"
	"call_rcu_sched"
	"kfree_call_rcu"
)
