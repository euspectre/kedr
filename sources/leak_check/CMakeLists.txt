# Name of the kernel module
set(kmodule_name "kedr_leak_check")
########################################################################

set(KEDR_GEN_LC_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")
kbuild_add_dependencies("kedr_base")

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
	to_abs_path(payload_data_file_abs ${payload_data_file})
	add_custom_command(OUTPUT ${payload_c_file}
		COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_LC_TEMPLATES_DIR}/payload.c/ ${payload_data_file_abs} > ${payload_c_file}
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
	"__alloc_pages_nodemask"
	"alloc_pages_current"
	"__free_pages"
	"alloc_pages_exact"
	"free_pages_exact" 
	
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
)

# This is to work around the fact that on some systems, *node*-functions
# are exported but still not available in the headers. A slower but more
# reliable way to detect if the functions can actually be used is employed
# here.
kmodule_configure_kernel_functions_hard(functions_node OPTIONAL 
	"__kmalloc_node"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmem_cache_alloc_node_trace"
)
list(APPEND functions ${functions_node})

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"klc_output.c"
	"mbi_ops.c"
	"stack_trace.c"
)

rules_generate_payload()
rules_build_datafile(${functions})
########################################################################

kedr_install_kmodule(${kmodule_name})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${leak_check_conf_file}"
	DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}") 
########################################################################

kedr_test_add_subdirectory(tests)
########################################################################
