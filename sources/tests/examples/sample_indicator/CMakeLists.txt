set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/examples/sample_indicator")

add_subdirectory(module)

example_create_build_script("build_example.sh" sample_indicator)
kedr_test_install(PROGRAMS "build_example.sh")

set(EXAMPLE_TEST_LOCATION "build")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

kedr_test_add_script("examples.sample_indicator.01" "test.sh")
