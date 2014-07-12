if(KERNEL_PART)
	add_subdirectory(module)
endif(KERNEL_PART)

if(NOT USER_PART)
	return()
endif(NOT USER_PART)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script("kedr_trace.simple_ordering.01" "test.sh")

kedr_test_install(PROGRAMS "verify_trace.awk")