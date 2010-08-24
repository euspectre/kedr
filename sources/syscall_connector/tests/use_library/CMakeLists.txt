configure_file("test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY)

kedr_test_add_script("syscall_connector.use_library.01" "test.sh")

subdirs(kernel_part)
subdirs(user_part)
