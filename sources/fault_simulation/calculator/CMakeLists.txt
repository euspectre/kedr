#Configure calculator implementation for in-kedr usage
set(CALCULATOR_HEADER "kedr/fault_simulation/calculator.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/calculator.c.in"
	"${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
	@ONLY)

add_subdirectory(tests)