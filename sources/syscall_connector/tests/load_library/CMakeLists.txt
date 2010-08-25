add_executable("test_sc_load_library_use_library" "use_library.c")
target_link_libraries("test_sc_load_library_use_library" "syscall_connector_u")

kedr_test_add_target("test_sc_load_library_use_library")

configure_file("test_load_library.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_load_library.sh")

kedr_test_add_script("syscall_connector.load_library.01" "test_load_library.sh")

configure_file("test_load_library_without_module.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_load_library_without_module.sh")

kedr_test_add_script("syscall_connector.load_library.02" "test_load_library_without_module.sh")