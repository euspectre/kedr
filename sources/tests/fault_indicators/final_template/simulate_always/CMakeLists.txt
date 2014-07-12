#name of module created
set(kmodule_name "kedr_test_indicator_simulate_always")

if(KERNEL_PART)
	add_test_indicator(${kmodule_name})
endif(KERNEL_PART)

if(NOT USER_PART)
	return()
endif(NOT USER_PART)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script(fault_simulation_indicator_template.with_simulate.01
	"test.sh")