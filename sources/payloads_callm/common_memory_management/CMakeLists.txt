#name of module created
set(kmodule_name "kedr_cm_cmm")
#name of data file
set(kmodule_data_file "payload.data")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kedr_configure_data_pieces(functions_data REQUIRED
	"__kmalloc"
	"krealloc"
	"kfree"
	"kmem_cache_alloc"
	OPTIONAL "kmem_cache_alloc_node"
	"kmem_cache_alloc_notrace" REQUIRED
	"kmem_cache_free"
)

set(kmem_cache_alloc_FUNCTION "kmem_cache_alloc")
configure_file("kmem_cache_alloc.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc.data")

list(FIND functions_data "kmem_cache_alloc_notrace" kmem_cache_alloc_notrace_index)
if(NOT kmem_cache_alloc_notrace_index EQUAL -1)
	set(kmem_cache_alloc_FUNCTION "kmem_cache_alloc_notrace")
	configure_file("kmem_cache_alloc.data.in"
		"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_notrace.data")
endif(NOT kmem_cache_alloc_notrace_index EQUAL -1)

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload(${CMAKE_CURRENT_BINARY_DIR}/${kmodule_data_file}
						${CMAKE_CURRENT_BINARY_DIR})

rules_build_datafile(${kmodule_data_file} "beginning.data"
	${functions_data})

kedr_install_kmodule(${kmodule_name})