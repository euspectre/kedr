include(cmake_useful)

set(CONTROLLER_SYMVERS_FILE "${CMAKE_BINARY_DIR}/controller/Module.symvers")
kbuild_use_symbols(${CONTROLLER_SYMVERS_FILE})

set(MIST_GEN_EXECUTABLE "${CMAKE_BINARY_DIR}/mist_gen/mist_gen")
set(MIST_GEN_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates")
# Common commands for generate payload.h and trace_payload.h
function(rules_generate_payload data_file dest_dir)
	add_custom_command(OUTPUT ${dest_dir}/payload.c
				COMMAND ${MIST_GEN_EXECUTABLE} ${MIST_GEN_TEMPLATES_DIR}/payload.c/ ${data_file} > ${dest_dir}/payload.c
				DEPENDS ${data_file})
	add_custom_command(OUTPUT ${dest_dir}/trace_payload.h
				COMMAND ${MIST_GEN_EXECUTABLE} ${MIST_GEN_TEMPLATES_DIR}/trace_payload.h/ ${data_file} > ${dest_dir}/trace_payload.h
				DEPENDS ${data_file})
endfunction(rules_generate_payload data_file dest_dir)

#rules_build_datafile(data_file data_begin [data_func1...])
function(rules_build_datafile data_file)
	to_abs_path(data_funcs_abs ${ARGN})
	add_custom_command(OUTPUT ${data_file}
						COMMAND cat ${data_funcs_abs} >> ${data_file}
						DEPENDS ${data_funcs_abs})
endfunction(rules_build_datafile data_file)

#add_subdirectory(my_payload1)
add_subdirectory(common_memory_management)