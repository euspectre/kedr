add_subdirectory(module)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script("kedr_trace.simple_ordering.01" "test.sh")