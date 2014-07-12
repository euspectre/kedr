# Name of the module to create
set(kmodule_name "kedr_cm_cmm")

if(USER_PART)
	kedr_conf_callm_add_payload(${kmodule_name})
endif(USER_PART)

# The rest is for kernel part only.
if(NOT KERNEL_PART)
	return()
endif(NOT KERNEL_PART)

# The header part of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

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
)

# [NB] If we could output the arguments of the called functions to the trace 
# in any order, we would not need to bother with different .data files 
# for kmem_cache_alloc_*trace in SLAB and SLUB here. Func_db already takes
# care of the difference in the function signatures. 
set(KMCA_FLAVOUR "slub")
if (DEFINED KMCA_TRACE_SIZE_FIRST)
	# The order of arguments was different in SLAB allocator pre-3.7.
	set(KMCA_FLAVOUR "slab")
endif (DEFINED KMCA_TRACE_SIZE_FIRST)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_trace.data.${KMCA_FLAVOUR}"
	"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_trace.data"
	COPYONLY
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_node_trace.data.${KMCA_FLAVOUR}"
	"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_node_trace.data"
	COPYONLY
)

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})
