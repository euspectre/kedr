add_subdirectory(module_a)
add_subdirectory(module_b)
add_subdirectory(module_c)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_simple.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_simple.sh"
    @ONLY)

kedr_test_add_script("fault_simulation.base.simple.01"
    "test_simple.sh")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_rewrite_indicator.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_rewrite_indicator.sh"
    @ONLY)

kedr_test_add_script("fault_simulation.base.rewrite_indicator.01"
    "test_rewrite_indicator.sh")