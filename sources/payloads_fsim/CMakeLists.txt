kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
kbuild_add_dependencies("kedr_fault_simulation")

# Common name of the payload source file
set(payload_c_file "payload.c")
# Common name of data file
set(payload_data_file "payload.data")
# Common name of functions support source file
set(functions_support_file "functions_support.c")
# Common name of beginning part of the data-file
set(begin_data_file "beginning.data")

#  Commands for create call monitor payload module.
#
# ${begin_data_file} file should contain data, common for all functions(i.e., includes).
# For each function in ${functions} list file ${function}.data
# should contain information about given function and what is planned to output to the trace.
function(create_payload_fsim module_name functions)
	# How to compile module
	kbuild_add_module(${module_name}
		"payload.c"
		"functions_support.c")
	# How to get payload and functions_support sources
	to_abs_path(payload_data_file_abs ${payload_data_file})
	if(KEDR_ENABLE_CALLER_ADDRESS)
		set(payload_data_file_with_caller_address "${CMAKE_CURRENT_BINARY_DIR}/payload_with_caller_address.data")
		
		add_custom_command(OUTPUT "${payload_c_file}"
					COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/payload_fsim.c/ ${payload_data_file_with_caller_address} > ${payload_c_file}
					DEPENDS ${payload_data_file_with_caller_address})

		add_custom_command(OUTPUT "${payload_data_file_with_caller_address}"
					COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/payload_fsim_add_caller_address.data/ ${payload_data_file_abs} > ${payload_data_file_with_caller_address}
					DEPENDS ${payload_data_file_abs})

	else(KEDR_ENABLE_CALLER_ADDRESS)
		add_custom_command(OUTPUT "${payload_c_file}"
					COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/payload_fsim.c/ ${payload_data_file_abs} > ${payload_c_file}
					DEPENDS ${payload_data_file_abs})

	endif(KEDR_ENABLE_CALLER_ADDRESS)

	add_custom_command(OUTPUT "${functions_support_file}"
				COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ ${payload_data_file_abs} > ${functions_support_file}
				DEPENDS ${payload_data_file_abs})
	# How to get full payload data-file
	set(functions_data)
	foreach(func ${functions} ${ARGN})
		list(APPEND functions_data "${func}.data")
	endforeach(func ${functions} ${ARGN})
	to_abs_path(source_files_abs ${begin_data_file} ${functions_data})

	add_custom_command(OUTPUT ${payload_data_file_abs}
						COMMAND cat ${source_files_abs} > ${payload_data_file_abs}
						DEPENDS ${source_files_abs})
endfunction(create_payload_fsim module_name functions)

add_subdirectory(common_memory_management)
add_subdirectory(user_space_access)
add_subdirectory(capable)
add_subdirectory(memory_utilities)
add_subdirectory(virtual_memory_management)

kedr_test_add_subdirectory(tests)