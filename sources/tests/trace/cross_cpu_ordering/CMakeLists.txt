add_subdirectory(module)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script("kedr_trace.cross_cpu_ordering.01" "test.sh")

kedr_test_install(PROGRAMS "verify_trace.awk")