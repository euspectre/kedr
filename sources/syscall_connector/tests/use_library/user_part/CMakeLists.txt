include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_executable("test_program" "test.c")
target_link_libraries("test_program" "syscall_connector_u")

kedr_test_add_target("test_program")