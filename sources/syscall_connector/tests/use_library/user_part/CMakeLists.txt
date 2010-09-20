include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_executable("test_sc_use_library_test_program" "test.c")
target_link_libraries("test_sc_use_library_test_program" "kedr_syscall_connector_u")

kedr_test_add_target("test_sc_use_library_test_program")