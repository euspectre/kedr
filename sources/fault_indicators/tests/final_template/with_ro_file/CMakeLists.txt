#name of module created
set(kmodule_name "kedr_test_indicator_ro_file")

add_test_indicator(${kmodule_name})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script(fault_simulation_indicator_template.ro_file.01
	"test.sh")