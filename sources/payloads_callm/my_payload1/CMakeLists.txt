#name of module created
set(kmodule_name "my_payload1")
#really harcoded, because of mist engine templates
#set(kmodule_source_file "payload.c")
#set(kmodule_header_file "trace_payload.h")
#name of data file
set(kmodule_data_file "my_payload1.data")

kmodule_configure_kernel_functions(functions_data
	"__kmalloc" 		"my_payload1___kmalloc.data"
	"kfree" 			"my_payload1_kfree.data"
	"kmem_cache_alloc" 	"my_payload1_kmem_cache_alloc.data"
	"kmem_cache_free" 	"my_payload1_kmem_cache_free.data")

if(NOT functions_data)
	message(FATAL_ERROR 
		"None of the functions of my_payload1.data exists in the kernel.")
endif(NOT functions_data)

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload(${CMAKE_CURRENT_BINARY_DIR}/${kmodule_data_file}
						${CMAKE_CURRENT_BINARY_DIR})

rules_build_datafile(${kmodule_data_file} "my_payload1_beginning.data"
	${functions_data})