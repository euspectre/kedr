# Name of the kernel module
set(kmodule_name "kedr_leak_check")
########################################################################

set(KEDR_GEN_LC_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr")

# Name of the data file to be created
set(payload_data_file "payload.data")

# Name of the header part of the data file
set(header_data_file "header.data")
configure_file("${header_data_file}.in"
	"${CMAKE_CURRENT_BINARY_DIR}/${header_data_file}")
########################################################################

# Configuration files: for installation and for testing
set(leak_check_conf_file "leak_check.conf")
set(leak_check_test_conf_file "leak_check_test.conf")
set(leak_check_install_dir ${KEDR_INSTALL_PREFIX_KMODULE})
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${leak_check_conf_file}.in"
	"${CMAKE_CURRENT_BINARY_DIR}/${leak_check_conf_file}"
	@ONLY)

#set(leak_check_install_dir ${CMAKE_CURRENT_BINARY_DIR})
#configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${leak_check_conf_file}.in"
#	"${CMAKE_CURRENT_BINARY_DIR}/${leak_check_test_conf_file}"
#	@ONLY)
########################################################################

# Commands to generate payload.c from ${payload_data_file} and to store 
# it in the current directory.
function(rules_generate_payload)
	set(payload_c_file "${CMAKE_CURRENT_BINARY_DIR}/payload.c")
	set(functions_support_file "${CMAKE_CURRENT_BINARY_DIR}/functions_support.c")
	to_abs_path(payload_data_file_abs ${payload_data_file})
	add_custom_command(OUTPUT ${payload_c_file}
		COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_LC_TEMPLATES_DIR}/payload.c/ ${payload_data_file_abs} > ${payload_c_file}
		DEPENDS ${payload_data_file_abs})
	add_custom_command(OUTPUT ${functions_support_file}
		COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ ${payload_data_file_abs} > ${functions_support_file}
		DEPENDS ${payload_data_file_abs})

endfunction(rules_generate_payload)

# Commands to generate ${payload_data_file} from ${header_data_file} 
# and the files corresponding to the specified functions.
# These files have the same names as the relevant functions and .data
# extension.
# The resulting file is stored in the current directory.
#
# Usage: rules_build_datafile(data_file func ...)
function(rules_build_datafile)
	set(functions_data)
	foreach(func ${ARGN})
		list(APPEND functions_data "${func}.data")
	endforeach(func ${ARGN})
	to_abs_path(source_files_abs ${header_data_file} ${functions_data})
	set(payload_data_file_abs "${CMAKE_CURRENT_BINARY_DIR}/${payload_data_file}")
	add_custom_command(OUTPUT ${payload_data_file_abs}
		COMMAND cat ${source_files_abs} > ${payload_data_file_abs}
		DEPENDS ${source_files_abs})
endfunction(rules_build_datafile)

rule_copy_file("stack_trace.c"
	"${CMAKE_SOURCE_DIR}/util/stack_trace/stack_trace.c")

# Check if kfree_rcu() is present and set HAVE_KFREE_RCU accordingly
check_kfree_rcu()

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

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"functions_support.c"
	
	"klc_output.c"
	"mbi_ops.c"
	"stack_trace.c"
)

rules_generate_payload()
rules_build_datafile(${functions})

check_allocator()
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_trace.data"
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_trace.data.${KERNEL_MEMORY_ALLOCATOR}")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_node_trace.data"
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_node_trace.data.${KERNEL_MEMORY_ALLOCATOR}")
########################################################################

kedr_install_kmodule(${kmodule_name})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${leak_check_conf_file}"
	DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}") 
########################################################################

kedr_test_add_subdirectory(tests)
########################################################################
