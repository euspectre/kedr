set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

file(WRITE ${function_triggers_file})

add_subdirectory(trigger_module)
add_subdirectory(trigger_program)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in
		${CMAKE_CURRENT_BINARY_DIR}/test.sh
		@ONLY)
