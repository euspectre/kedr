set(program_name trigger_program)
set(source_file "${CMAKE_CURRENT_BINARY_DIR}/${program_name}.c")

add_custom_command(OUTPUT ${source_file}
				COMMAND ${KEDR_GEN_TOOL} "${templates_dir}/trigger_program.c" ${function_triggers_file} > ${source_file}
				DEPENDS ${function_triggers_file})

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(${program_name} ${source_file})

kedr_test_add_target(${program_name})
