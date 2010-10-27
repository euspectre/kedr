# Share templates with fault simulation
set(KEDR_GEN_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/payloads_fsim/templates/") 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")

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

set(triggers_include_filename "triggers_include.list")
# Add include directives, needed for triggers for functions
# from ${triggers_include_filename} file
# (in the current source or binary directory)
# kedr_test_add_triggers_include()
function(kedr_test_add_triggers_include)
	file(APPEND ${function_triggers_file} "include =>>\n")
	to_abs_path(triggers_include_file "${triggers_include_filename}")
	file(READ "${triggers_include_file}" triggers_include_code)
	file(APPEND ${function_triggers_file} "${triggers_include_code}" "\n")
	file(APPEND ${function_triggers_file} "<<\n")
endfunction(kedr_test_add_triggers_include)
# Add triggers and tests for given functions in the payload module
# For each function 'f' its trigger code should be in the file 'f.trigger'
# kedr_test_add_trigger_for_function(payload function ...)
function(kedr_test_add_trigger_for_function payload)
	foreach(function ${ARGN})
		file(APPEND ${function_triggers_file} "function = ${function}\n")
		file(APPEND ${function_triggers_file} "payload = ${payload}\n")
		file(APPEND ${function_triggers_file} "code =>>\n")
		to_abs_path(trigger_file "${function}.trigger")
		file(READ "${trigger_file}" trigger_code)
		file(APPEND ${function_triggers_file} "${trigger_code}" "\n")
		file(APPEND ${function_triggers_file} "<<\n")

		kedr_test_add_script("controller.call_interception.${function}.01"
				"${CMAKE_BINARY_DIR}/controller/tests/call_interception/test.sh"
				"${function}"
				"${payload}"
		)
	endforeach(function ${ARGN})
endfunction(kedr_test_add_trigger_for_function payload)

# Macro for added load of given payload into default configuration file for call monitor payloads
macro(kedr_add_default_cm_payload payload_name)
    file(APPEND ${kedr_cm_conf_file_data} "name = ${payload_name}\n")
    file(APPEND ${kedr_cm_conf_file_data} "install_path = ${KEDR_INSTALL_PREFIX_KMODULE}/${payload_name}.ko\n")
    file(APPEND ${kedr_cm_conf_file_data} "test_path = ${CMAKE_CURRENT_BINARY_DIR}/${payload_name}.ko\n")
endmacro(kedr_add_default_cm_payload payload_name)

add_subdirectory(common_memory_management)
add_subdirectory(mutexes)
add_subdirectory(spinlocks)
add_subdirectory(user_space_access)
add_subdirectory(schedule)
add_subdirectory(waitqueue)
add_subdirectory(capable)
add_subdirectory(virtual_memory_management)

