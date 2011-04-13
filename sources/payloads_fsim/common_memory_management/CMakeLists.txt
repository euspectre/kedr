#name of module created
set(kmodule_name "kedr_fsim_cmm")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

# free()-like functions are only monitored (they cannot fail anyway)
kmodule_configure_kernel_functions(functions REQUIRED
	"__kmalloc"
	"krealloc"
	"__krealloc"
	"kmem_cache_alloc"
	OPTIONAL "kmem_cache_alloc_notrace"
	"kmem_cache_alloc_trace" 
	"__get_free_pages"
	"get_zeroed_page"
	OPTIONAL "__alloc_pages_nodemask"
	"alloc_pages_current"
	"alloc_pages_exact"
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

set(kmem_cache_alloc_FUNCTION "kmem_cache_alloc")
configure_file("kmem_cache_alloc.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc.data")

list(FIND functions "kmem_cache_alloc_notrace" kmem_cache_alloc_notrace_index)
if(NOT kmem_cache_alloc_notrace_index EQUAL -1)
	set(kmem_cache_alloc_FUNCTION "kmem_cache_alloc_notrace")
	configure_file("kmem_cache_alloc.data.in"
		"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_notrace.data")
endif(NOT kmem_cache_alloc_notrace_index EQUAL -1)

create_payload_fsim(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_fsim_add_payload(${kmodule_name})
