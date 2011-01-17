set(KEDR_GEN_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")
kbuild_add_dependencies("kedr_base")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
kbuild_add_dependencies("kedr_fault_simulation")

# Common name of data file
set(indicator_data_file "indicator.data")

# Commands for generate indicator.c and place it into current directory
# from ${indicator_data_file}, placed in the current source or binary directory.
function(rules_generate_indicator)
	set(indicator_c_file "${CMAKE_CURRENT_BINARY_DIR}/indicator.c")
	set(indicator_data_file_internal "${CMAKE_CURRENT_BINARY_DIR}/indicator_internal.data")
	to_abs_path(indicator_data_file_abs ${indicator_data_file})
	add_custom_command(OUTPUT ${indicator_c_file}
		COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/indicator.c/ ${indicator_data_file_internal} > ${indicator_c_file}
		DEPENDS ${indicator_data_file_internal})
	if(KEDR_FSIM_CALLER_ADDRESS)
		set(caller_address_data_file "${CMAKE_CURRENT_BINARY_DIR}/indicator_called_address_internal.data")
		add_custom_command(OUTPUT ${indicator_data_file_internal}
			COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/indicator_internal.data/ ${caller_address_data_file} > ${indicator_data_file_internal}
			DEPENDS ${caller_address_data_file})
		add_custom_command(OUTPUT ${caller_address_data_file}
			COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/indicator_caller_address_internal.data/ ${indicator_data_file_abs} > ${caller_address_data_file}
			DEPENDS ${indicator_data_file_abs})
	else(KEDR_FSIM_CALLER_ADDRESS)
		add_custom_command(OUTPUT ${indicator_data_file_internal}
			COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/indicator_internal.data/ ${indicator_data_file_abs} > ${indicator_data_file_internal}
			DEPENDS ${indicator_data_file_abs})
	endif(KEDR_FSIM_CALLER_ADDRESS)
	
endfunction(rules_generate_indicator)

add_subdirectory(common_memory_management)
add_subdirectory(capable)
add_subdirectory(common)

add_subdirectory(tests)

