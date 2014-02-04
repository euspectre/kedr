# Name of the kernel module
set(kmodule_name "kedr_lc_common_mm")
########################################################################

# Additional symbols to use. Note that the symbols from KEDR Core are
# made available to the payload modules automatically if 
# kedr_create_payload_module() is used to create them.
# The dependency on KEDR Core is also set automatically for such modules.
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
	"__page_cache_alloc"

	OPTIONAL "posix_acl_alloc"
	"posix_acl_clone"
	"posix_acl_from_mode"
	"posix_acl_from_xattr"

	OPTIONAL "match_strdup"
	
	OPTIONAL "kasprintf"
	"kvasprintf"
)

# TODO: see the notes and TODO in sources/functions/mem_util/CMakelists.txt
kmodule_configure_kernel_functions(posix_acl_changed
	OPTIONAL "__posix_acl_create"
	"__posix_acl_chmod"
)

if (posix_acl_changed)
	list(APPEND functions ${posix_acl_changed})
else(posix_acl_changed)
	kmodule_configure_kernel_functions(posix_acl_functions
		OPTIONAL "posix_acl_create"
		"posix_acl_chmod"
	)
	if (posix_acl_functions)
		list(APPEND functions ${posix_acl_functions})
	endif (posix_acl_functions)
endif(posix_acl_changed)

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
########################################################################

create_payload_leak_check(${kmodule_name} ${functions})
kedr_install_kmodule(${kmodule_name})
########################################################################
