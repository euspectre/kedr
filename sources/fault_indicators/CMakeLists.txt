kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
kbuild_add_dependencies("kedr_fault_simulation")

# Common name of data file
set(indicator_data_file "indicator.data")

# Commands for create indicator module from
# ${indicator_data_file}, placed in the current source or binary directory.
function(create_indicator module_name)
	kbuild_add_module(${module_name} 
		"indicator.c" 
		"calculator.c" 
		"control_file.c")

	rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
		"${CMAKE_SOURCE_DIR}/calculator/calculator.c")

	rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
		"${CMAKE_SOURCE_DIR}/control_file/control_file.c")

	set(indicator_c_file "${CMAKE_CURRENT_BINARY_DIR}/indicator.c")
	set(indicator_data_file_internal "${CMAKE_CURRENT_BINARY_DIR}/indicator_internal.data")
	to_abs_path(indicator_data_file_abs ${indicator_data_file})
	add_custom_command(OUTPUT ${indicator_c_file}
		COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/fault_indicator.c/ ${indicator_data_file_internal} > ${indicator_c_file}
		DEPENDS ${indicator_data_file_internal})
	if(KEDR_ENABLE_CALLER_ADDRESS)
		set(indicator_data_file_with_caller_address "${CMAKE_CURRENT_BINARY_DIR}/indicator_with_caller_address.data")
		add_custom_command(OUTPUT ${indicator_data_file_with_caller_address}
			COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/fault_indicator_add_caller_address.data/ ${indicator_data_file_abs} > ${indicator_data_file_with_caller_address}
			DEPENDS ${indicator_data_file_abs})

		add_custom_command(OUTPUT ${indicator_data_file_internal}
			COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/fault_indicator.data/ ${indicator_data_file_with_caller_address} > ${indicator_data_file_internal}
			DEPENDS ${indicator_data_file_with_caller_address})

	else(KEDR_ENABLE_CALLER_ADDRESS)
		add_custom_command(OUTPUT ${indicator_data_file_internal}
			COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/fault_indicator.data/ ${indicator_data_file_abs} > ${indicator_data_file_internal}
			DEPENDS ${indicator_data_file_abs})

	endif(KEDR_ENABLE_CALLER_ADDRESS)
endfunction(create_indicator module_name)

add_subdirectory(common_memory_management)
add_subdirectory(capable)
add_subdirectory(common)

kedr_test_add_subdirectory(tests)

