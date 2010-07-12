#name of module created
set(kmodule_name "kedr_cm_cmm")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"__kmalloc"
	"krealloc"
	"kfree"
	"kmem_cache_alloc"
	OPTIONAL "kmem_cache_alloc_notrace" REQUIRED
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

rules_generate_payload()

rules_build_datafile(${functions})

kedr_install_kmodule(${kmodule_name})

kedr_add_default_cm_payload(${kmodule_name})