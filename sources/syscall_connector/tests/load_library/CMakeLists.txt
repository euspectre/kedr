add_executable("use_library" "use_library.c")
target_link_libraries("use_library" "syscall_connector_u")

kedr_test_add_target("use_library")

configure_file("test_load_library.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_load_library.sh")

kedr_test_add_script("syscall_connector.load_library.01" "test_load_library.sh")