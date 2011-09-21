#name of module created
set(kmodule_name "kedr_cm_cmm")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"__kmalloc"
	"krealloc"
	"__krealloc"
	"kfree"
	"kzfree"
	"kmem_cache_alloc"
	OPTIONAL "kmem_cache_alloc_notrace" 
	"kmem_cache_alloc_trace" 
	"__kmalloc_node"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmem_cache_alloc_node_trace"
	"kmalloc_order_trace"
	REQUIRED "kmem_cache_free"
	"__get_free_pages"
	"get_zeroed_page"
	"free_pages"
	OPTIONAL "__alloc_pages_nodemask"
	"alloc_pages_current"
	"__free_pages"
	"alloc_pages_exact"
	"free_pages_exact"
	"alloc_pages_exact_nid"
#	"__kmalloc_track_caller"
#	"__kmalloc_node_track_caller"
)

set(kmem_cache_alloc_FUNCTION "kmem_cache_alloc")
configure_file("kmem_cache_alloc.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc.data")

list(FIND functions "kmem_cache_alloc_notrace" kmem_cache_alloc_notrace_index)
if(NOT kmem_cache_alloc_notrace_index EQUAL -1)
	set(kmem_cache_alloc_FUNCTION "kmem_cache_alloc_notrace")
	configure_file("kmem_cache_alloc.data.in"
		"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_notrace.data")
endif(NOT kmem_cache_alloc_notrace_index EQUAL -1)

check_allocator()
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_trace.data"
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_trace.data.${KERNEL_MEMORY_ALLOCATOR}")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_node_trace.data"
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_node_trace.data.${KERNEL_MEMORY_ALLOCATOR}")

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})

kedr_test_add_subdirectory(tests)
