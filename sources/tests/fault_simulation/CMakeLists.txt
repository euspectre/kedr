set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/fault_simulation")

if(KERNEL_PART)
    add_subdirectory(module_a)
    add_subdirectory(module_b)
    add_subdirectory(indicators_simple)
endif(KERNEL_PART)

if(NOT USER_PART)
    return()
endif(NOT USER_PART)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_simple.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_simple.sh"
    @ONLY)

kedr_test_add_script("fault_simulation.simple.01"
    "test_simple.sh")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_fault_tolerance.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_fault_tolerance.sh"
    @ONLY)

kedr_test_add_script("fault_simulation.fault_tolerance.01"
    "test_fault_tolerance.sh")

# TODO: What about "test_rewrite_indicator.sh.in" ??