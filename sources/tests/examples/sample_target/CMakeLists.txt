if(NOT USER_PART)
	return()
endif(NOT USER_PART)

example_create_build_script(test_example_sample_target "build_example.sh")
kedr_test_install(PROGRAMS "build_example.sh")

set(EXAMPLE_TEST_LOCATION "build")

add_executable(test_sample_target test_sample_target.c)
# Install executable created as other scripts.
kedr_test_install(PROGRAMS "test_sample_target")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/load_unload_simple.sh.in
	${CMAKE_CURRENT_BINARY_DIR}/load_unload_simple.sh
	@ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/load_unload_with_test.sh.in
	${CMAKE_CURRENT_BINARY_DIR}/load_unload_with_test.sh
	@ONLY)

kedr_test_add_script(examples.sample_target.01
	${CMAKE_CURRENT_BINARY_DIR}/load_unload_simple.sh)

kedr_test_add_script(examples.sample_target.02
	${CMAKE_CURRENT_BINARY_DIR}/load_unload_with_test.sh)
