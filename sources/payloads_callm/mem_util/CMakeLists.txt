# Name of the module to be created
set(kmodule_name "kedr_cm_mem_util")

# The header of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data"
)

kmodule_configure_kernel_functions(functions REQUIRED
	"kstrdup"
	"kstrndup"
	"kmemdup"
	
	OPTIONAL "add_to_page_cache_lru"
	"add_to_page_cache_locked"

	OPTIONAL "posix_acl_alloc"
	"posix_acl_clone"
	"posix_acl_from_mode"

	OPTIONAL "match_strdup"
)
# N.B. add_to_page_cache_* is to be intercepted only to handle some
# situations where the repsonsibility for a page is transferred from 
# the target kernel module (e.g. some NFS module) to the kernel proper.

# Check if kfree_rcu() is present and set HAVE_KFREE_RCU accordingly
check_kfree_rcu()

# N.B. kfree_rcu() uses call_rcu/call_rcu_sched to call kfree() on an 
# "RCU-protected" structure after the grace period has expired. So it makes 
# sense to track the calls to these functions too if kfree_rcu is available.
if (HAVE_KFREE_RCU)
	kmodule_configure_kernel_functions(call_rcu_functions 
		OPTIONAL "call_rcu"
		"call_rcu_sched"
		"kfree_call_rcu"
	)
	if (NOT call_rcu_functions)
		message("\n[WARNING]\nThe system provides kfree_rcu() but "
			"the corresponding call_rcu* functions are not available.\n"
			"kfree_rcu() might have been implemented here in some other way. "
			"Please report this problem to KEDR developers.\n"
		)
	endif (NOT call_rcu_functions)
	list(APPEND functions ${call_rcu_functions})
endif (HAVE_KFREE_RCU)

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})
