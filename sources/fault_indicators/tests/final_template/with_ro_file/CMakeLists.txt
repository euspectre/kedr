#name of module created
set(kmodule_name "kedr_test_indicator_ro_file")

kbuild_add_module(${kmodule_name} "indicator.c" "control_file.c")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
	"${CMAKE_BINARY_DIR}/control_file/control_file.c")

rules_generate_internal_indicator()

kedr_test_add_target(${kmodule_name})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script(fault_simulation_indicator_template.ro_file.01
	"test.sh")