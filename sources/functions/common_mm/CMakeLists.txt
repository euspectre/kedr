# Some functions have different signatures depending on the memory
# allocator used in the kernel.
check_allocator()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_trace.data.${KERNEL_MEMORY_ALLOCATOR}"
	"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_trace.data"
	COPYONLY
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/kmem_cache_alloc_node_trace.data.${KERNEL_MEMORY_ALLOCATOR}"
	"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_node_trace.data"
	COPYONLY
)

# Make the information about the functions listed here available to
# other components of KEDR.
func_db_configure_functions (
	"__kmalloc"
	"krealloc"
	"__krealloc"
	"kfree"
	"kzfree"
	"kmem_cache_alloc"
	"kmem_cache_alloc_notrace" 
	"__kmalloc_node"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmalloc_order_trace"
	"kmem_cache_free"
	"__get_free_pages"
	"get_zeroed_page"
	"free_pages"
	"__alloc_pages_nodemask"
	"alloc_pages_current"
	"__free_pages"
	"alloc_pages_exact"
	"free_pages_exact"
	"alloc_pages_exact_nid"
	
	GENERATED
	"kmem_cache_alloc_trace"
	"kmem_cache_alloc_node_trace"
)
