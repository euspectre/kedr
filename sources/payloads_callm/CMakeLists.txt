kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/trace/Module.symvers")
kbuild_add_dependencies("kedr_trace")

# Common name of the payload source file
set(payload_c_file "payload.c")
# Common name of data file
set(payload_data_file "payload.data")
# Common name of functions support source file
set(functions_support_file "functions_support.c")
# Common name of beginning part of the data-file
set(begin_data_file "beginning.data")

# Commands to create a payload module for call monitoring.
#
# ${begin_data_file} file should contain data, common for all functions(i.e., includes).
# For each function in ${functions} list file ${function}.data
# should contain information about given function and what is planned to output to the trace.
function(create_payload_callm module_name functions)
	# How to compile module
	kbuild_add_module(${module_name}
		"payload.c"
		"functions_support.c")
	# How to get payload and functions_support sources
	to_abs_path(payload_data_file_abs ${payload_data_file})
	add_custom_command(OUTPUT "${payload_c_file}"
				COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/payload_callm.c/ ${payload_data_file_abs} > ${payload_c_file}
				DEPENDS ${payload_data_file_abs})
	add_custom_command(OUTPUT "${functions_support_file}"
				COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ ${payload_data_file_abs} > ${functions_support_file}
				DEPENDS ${payload_data_file_abs})
	# How to get full payload data-file
	set(functions_data)
	foreach(func ${functions} ${ARGN})
		list(APPEND functions_data "${func}.data")
	endforeach(func ${functions} ${ARGN})
	to_abs_path(source_files_abs ${begin_data_file} ${functions_data})
	set(payload_data_file_abs "${CMAKE_CURRENT_BINARY_DIR}/${payload_data_file}")
	add_custom_command(OUTPUT ${payload_data_file_abs}
						COMMAND cat ${source_files_abs} > ${payload_data_file_abs}
						DEPENDS ${source_files_abs})
endfunction(create_payload_callm module_name functions)

#'tests' subdirectory define function which is used by payloads,
# so this directory should be first
kedr_test_add_subdirectory(tests)

add_subdirectory(common_memory_management)
add_subdirectory(mutexes)
add_subdirectory(spinlocks)
add_subdirectory(user_space_access)
add_subdirectory(schedule)
add_subdirectory(waitqueue)
add_subdirectory(capable)
add_subdirectory(virtual_memory_management)
add_subdirectory(memory_utilities)
