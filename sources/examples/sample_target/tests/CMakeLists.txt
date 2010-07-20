add_executable(test_sample_target EXCLUDE_FROM_ALL test_sample_target.c)

add_dependencies(build_tests test_sample_target)

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