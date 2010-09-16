configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_unref.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_unref.sh"
    @ONLY
)

kedr_test_add_script("module_weak_ref.simple.01"
    "test.sh")

kedr_test_add_script("module_weak_ref.simple.02"
    "test_unref.sh")

add_subdirectory(module_a)
add_subdirectory(module_b)
add_subdirectory(module_c)
