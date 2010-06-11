#name of module created
set(kmodule_name "kedr_cm_cmm")
#really harcoded, because of mist engine templates
#set(kmodule_source_file "payload.c")
#set(kmodule_header_file "trace_payload.h")
#name of data file
set(kmodule_data_file "my_payload1.data")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

#__kmalloc
kmodule_configure_kernel_functions(__kmalloc_data
	"__kmalloc" 		"__kmalloc.data")
if(NOT __kmalloc_data)
	message(FATAL_ERROR "__kmalloc is absent in the kernel.")
endif(NOT __kmalloc_data)
#kfree
kmodule_configure_kernel_functions(kfree_data
	"kfree" 		"kfree.data")
if(NOT kfree_data)
	message(FATAL_ERROR "kfree is absent in the kernel.")
endif(NOT kfree_data)
#kmem_cache_alloc
kmodule_configure_kernel_functions(kmem_cache_alloc_data
	"kmem_cache_alloc" 		"kmem_cache_alloc.data")
if(NOT kmem_cache_alloc_data)
	message(FATAL_ERROR "kmem_cache_alloc is absent in the kernel.")
endif(NOT kmem_cache_alloc_data)

set(kmem_cache_alloc_FUNCTION "kmem_cache_alloc")
configure_file("kmem_cache_alloc.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc.data")
#kmem_cache_alloc_notrace(optional)
kmodule_configure_kernel_functions(kmem_cache_alloc_notrace_data
	"kmem_cache_alloc_notrace" 		"kmem_cache_alloc_notrace.data")
if(kmem_cache_alloc_notrace_data)
	set(kmem_cache_alloc_FUNCTION "kmem_cache_alloc_notrace")
	configure_file("kmem_cache_alloc.data.in"
		"${CMAKE_CURRENT_BINARY_DIR}/kmem_cache_alloc_notrace.data")
endif(kmem_cache_alloc_notrace_data)

#kmem_cache_free
kmodule_configure_kernel_functions(kmem_cache_free_data
	"kmem_cache_free" 		"kmem_cache_free.data")
if(NOT kmem_cache_free_data)
	message(FATAL_ERROR "kmem_cache_free is absent in the kernel.")
endif(NOT kmem_cache_free_data)


set(functions_data
	${__kmalloc_data}
	${kfree_data}
	${kmem_cache_alloc_data}
	${kmem_cache_alloc_notrace_data}
	${kmem_cache_free_data}
	)


kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload(${CMAKE_CURRENT_BINARY_DIR}/${kmodule_data_file}
						${CMAKE_CURRENT_BINARY_DIR})

rules_build_datafile(${kmodule_data_file} "beginning.data"
	${functions_data})

kedr_install_kmodule(${kmodule_name})