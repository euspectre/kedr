# Name of the kernel module
set(kmodule_name "kedr_lc_common_mm")
########################################################################

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/leak_check/core/Module.symvers")
kbuild_add_dependencies("kedr_leak_check")
########################################################################

# Name of the header part of the data file
configure_file(
	"header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

kmodule_configure_kernel_functions(functions 
	REQUIRED "__kmalloc"
	"krealloc"
	"__krealloc"
	"kfree"
	"kzfree"
	"kmem_cache_alloc"
	"kmem_cache_free"
	"__get_free_pages"
	"get_zeroed_page"
	"free_pages"
	OPTIONAL "kmem_cache_alloc_notrace"
	"kmem_cache_alloc_trace"
	"__kmalloc_node"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmem_cache_alloc_node_trace"
	"kmalloc_order_trace"
	"__alloc_pages_nodemask"
	"alloc_pages_current"
	"__free_pages"
	"alloc_pages_exact"
	"free_pages_exact" 
	"alloc_pages_exact_nid"
#	"__kmalloc_track_caller"
#	"__kmalloc_node_track_caller"
	
	REQUIRED "kstrdup"
	"kstrndup"
	"kmemdup"
	
	REQUIRED "strndup_user"
	"memdup_user" 
	
	REQUIRED "vmalloc"
	"__vmalloc"
	"vmalloc_user"
	"vmalloc_node"
	"vmalloc_32"
	"vmalloc_32_user"
	"vfree"

	OPTIONAL "vzalloc"
	"vzalloc_node"

# add_to_page_cache* is handled similar to free_pages() here to avoid
# false positives when analysing NFS modules or the like. These modules
# allocate memory pages and sometimes transfer the responsibility for 
# them to the kernel proper among other things.
	OPTIONAL "add_to_page_cache_lru"
	"add_to_page_cache_locked"

	OPTIONAL "posix_acl_alloc"
	"posix_acl_clone"
	"posix_acl_from_mode"

	OPTIONAL "match_strdup"
)

# N.B. kfree_rcu() uses call_rcu/call_rcu_sched to call kfree() on an 
# "RCU-protected" structure after the grace period has expired. So it makes 
# sense to track the calls to these functions too if kfree_rcu is available.
if (HAVE_KFREE_RCU)
	kmodule_configure_kernel_functions(call_rcu_functions 
		OPTIONAL "call_rcu"
		"call_rcu_sched"
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

check_allocator()
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_trace.data"
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_trace.data.${KERNEL_MEMORY_ALLOCATOR}")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_node_trace.data"
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_node_trace.data.${KERNEL_MEMORY_ALLOCATOR}")
########################################################################

create_payload_leak_check(${kmodule_name} ${functions})
kedr_install_kmodule(${kmodule_name})
########################################################################
