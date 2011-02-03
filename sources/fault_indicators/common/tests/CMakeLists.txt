configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script("fault_indicators.common.01"
	"test.sh")

add_subdirectory(module)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_caller_address.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_caller_address.sh"
	@ONLY)

kedr_test_add_script("fault_indicators.common.02"
	"test_caller_address.sh")

add_subdirectory(target_caller_address)

