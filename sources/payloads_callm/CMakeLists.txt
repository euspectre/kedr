kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")

set(MIST_GEN_EXECUTABLE "${CMAKE_BINARY_DIR}/mist_gen/mist_gen")
set(MIST_GEN_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates")

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
				COMMAND ${MIST_GEN_EXECUTABLE} ${MIST_GEN_TEMPLATES_DIR}/payload.c/ ${payload_data_file_abs} > ${payload_c_file}
				DEPENDS ${payload_data_file_abs})
	add_custom_command(OUTPUT ${trace_payload_h_file}
				COMMAND ${MIST_GEN_EXECUTABLE} ${MIST_GEN_TEMPLATES_DIR}/trace_payload.h/ ${payload_data_file_abs} > ${trace_payload_h_file}
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

#for local use, add line to configuration file, for install one and for test one
macro(kedr_add_conf_line_install)
    file(APPEND ${kedr_cm_conf_file} ${ARGN} "\n")
endmacro(kedr_add_conf_line_install)
macro(kedr_add_conf_line_test)
    file(APPEND ${kedr_test_cm_conf_file} ${ARGN} "\n")
endmacro(kedr_add_conf_line_test)

# For both configuration files(common lines)
macro(kedr_add_conf_line)
    kedr_add_conf_line_install(${ARGN})
    kedr_add_conf_line_test(${ARGN})
endmacro(kedr_add_conf_line)

# 
macro(kedr_add_default_cm_payload payload_name)
    kedr_add_conf_line_install("payload ${KEDR_INSTALL_PREFIX_KMODULE}/${payload_name}.ko")
    kedr_add_conf_line_test("payload ${CMAKE_CURRENT_BINARY_DIR}/${payload_name}.ko")
# Enable tracepoints
    kedr_add_conf_line("on_load echo 1 > ${kedr_debugfs_dir}/tracing/events/${payload_name}/enable")
endmacro(kedr_add_default_cm_payload payload_name)

kedr_add_conf_line("on_load mkdir -p ${kedr_debugfs_dir}")
kedr_add_conf_line("on_load mount debugfs -t debugfs ${kedr_debugfs_dir}")
kedr_add_conf_line("on_unload umount ${kedr_debugfs_dir}")

add_subdirectory(common_memory_management)
add_subdirectory(mutexes)
add_subdirectory(spinlocks)
add_subdirectory(user_space_access)
add_subdirectory(schedule)
add_subdirectory(waitqueue)
add_subdirectory(capable)
add_subdirectory(virtual_memory_management)

# File to store result of capturing trace (path relative to 'var' directory)
set(trace_result_subdir "trace_result")
set(trace_result_file "${trace_result_subdir}/trace")

kedr_add_conf_line_install("on_load mkdir -p ${KEDR_INSTALL_PREFIX_VAR}/${trace_result_subdir}")
kedr_add_conf_line_install("on_load ${KEDR_INSTALL_PREFIX_EXEC}/capture_trace start ${KEDR_INSTALL_PREFIX_VAR}/${trace_result_file} ${kedr_debugfs_dir}")
kedr_add_conf_line_install("on_unload ${KEDR_INSTALL_PREFIX_EXEC}/capture_trace stop")

kedr_add_conf_line_test("on_load mkdir -p ${CMAKE_BINARY_DIR}/var/${trace_result_subdir}")
kedr_add_conf_line_test("on_load sh ${CMAKE_BINARY_DIR}/tools/capture_trace/capture_trace start ${CMAKE_BINARY_DIR}/var/${trace_result_file} ${kedr_debugfs_dir}")
kedr_add_conf_line_test("on_unload sh ${CMAKE_BINARY_DIR}/tools/capture_trace/capture_trace stop")

