set(KEDR_GEN_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")

# Common name of data file
set(payload_data_file "payload.data")
# Common name of beginning part of the data-file
set(begin_data_file "beginning.data")

# Commands for generate payload.c and trace_payload.h and place them into current directory
# from ${payload_data_file}, placed in the current source or binary directory.
function(rules_generate_payload)
	set(payload_c_file "${CMAKE_CURRENT_BINARY_DIR}/payload.c")
	set(trace_payload_h_file "${CMAKE_CURRENT_BINARY_DIR}/trace_payload.h")
	to_abs_path(payload_data_file_abs ${payload_data_file})
	add_custom_command(OUTPUT ${payload_c_file}
				COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/payload.c/ ${payload_data_file_abs} > ${payload_c_file}
				DEPENDS ${payload_data_file_abs})
	add_custom_command(OUTPUT ${trace_payload_h_file}
				COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/trace_payload.h/ ${payload_data_file_abs} > ${trace_payload_h_file}
				DEPENDS ${payload_data_file_abs})
endfunction(rules_generate_payload)

# Commands for generate ${payload_data_file} from ${begin_data_file} and all files,
# getting from function names by appending ".data" extension to them.
# Result is placed into current directory, sources should be placed in the current source or binary directory.
#
#rules_build_datafile(data_file func ...)
function(rules_build_datafile)
	set(functions_data)
	foreach(func ${ARGN})
		list(APPEND functions_data "${func}.data")
	endforeach(func ${ARGN})
	to_abs_path(source_files_abs ${begin_data_file} ${functions_data})
	set(payload_data_file_abs "${CMAKE_CURRENT_BINARY_DIR}/${payload_data_file}")
	add_custom_command(OUTPUT ${payload_data_file_abs}
						COMMAND cat ${source_files_abs} > ${payload_data_file_abs}
						DEPENDS ${source_files_abs})
endfunction(rules_build_datafile)

add_subdirectory(common_memory_management)
#add_subdirectory(mutexes)
#add_subdirectory(spinlocks)
add_subdirectory(user_space_access)
#add_subdirectory(schedule)
#add_subdirectory(waitqueue)
add_subdirectory(capable)
#add_subdirectory(virtual_memory_management)

