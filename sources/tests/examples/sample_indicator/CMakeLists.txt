set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/examples/sample_indicator")

if(KERNEL_PART)
	add_subdirectory(module)
endif(KERNEL_PART)

if(NOT USER_PART)
    return()
endif(NOT USER_PART)

example_create_build_script(test_example_sample_indicator "build_example.sh")
kedr_test_install(PROGRAMS "build_example.sh")

set(EXAMPLE_TEST_LOCATION "build")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script("examples.sample_indicator.01" "test.sh")
