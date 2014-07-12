# Name of the module to create
set(kmodule_name "kedr_fsim_cmm")

if(USER_PART)
	kedr_conf_fsim_add_payload(${kmodule_name})
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
	"kmem_cache_alloc"
	OPTIONAL "kmem_cache_alloc_notrace"
	"kmem_cache_alloc_trace"
	"__kmalloc_node"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmem_cache_alloc_node_trace"
	"kmalloc_order_trace"
	"__get_free_pages"
	"get_zeroed_page"
	OPTIONAL "__alloc_pages_nodemask"
	"alloc_pages_current"
	"alloc_pages_exact"
	"alloc_pages_exact_nid"
)

create_payload_fsim(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})


