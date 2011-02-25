set(KEDR_GEN_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")
kbuild_add_dependencies("kedr_base")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
kbuild_add_dependencies("kedr_fault_simulation")

# Common name of data file
set(indicator_data_file "indicator.data")

# Commands for create indicator module from
# ${indicator_data_file}, placed in the current source or binary directory.
function(create_indicator module_name)
	if (KEDR_ENABLE_CALLER_ADDRESS)
		kbuild_add_module(${module_name} 
			"indicator.c" 
			"calculator.c" 
			"control_file.c"
			"stack_trace.c")
		
		rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/stack_trace.c"
			"${CMAKE_SOURCE_DIR}/util/stack_trace/stack_trace.c")
	else (KEDR_ENABLE_CALLER_ADDRESS)
		kbuild_add_module(${module_name} 
			"indicator.c" 
			"calculator.c" 
			"control_file.c")
	endif (KEDR_ENABLE_CALLER_ADDRESS)

	rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
		"${CMAKE_SOURCE_DIR}/calculator/calculator.c")

	rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
		"${CMAKE_SOURCE_DIR}/control_file/control_file.c")

	set(indicator_c_file "${CMAKE_CURRENT_BINARY_DIR}/indicator.c")
	set(indicator_data_file_internal "${CMAKE_CURRENT_BINARY_DIR}/indicator_internal.data")
	to_abs_path(indicator_data_file_abs ${indicator_data_file})
	add_custom_command(OUTPUT ${indicator_c_file}
		COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/indicator.c/ ${indicator_data_file_internal} > ${indicator_c_file}
		DEPENDS ${indicator_data_file_internal})
	add_custom_command(OUTPUT ${indicator_data_file_internal}
		COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/indicator_internal.data/ ${indicator_data_file_abs} > ${indicator_data_file_internal}
		DEPENDS ${indicator_data_file_abs})
	
endfunction(create_indicator module_name)

add_subdirectory(common_memory_management)
add_subdirectory(capable)
add_subdirectory(common)

add_subdirectory(tests)

