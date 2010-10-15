#name of module created
set(kmodule_name "kedr_fsim_indicator_common_memory_management")

kbuild_add_module(${kmodule_name} "indicator.c" "calculator.c" "control_file.c")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
	"${CMAKE_BINARY_DIR}/fault_simulation/calculator/calculator.c")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
	"${CMAKE_BINARY_DIR}/control_file/control_file.c")


rules_generate_indicator()

kedr_install_kmodule(${kmodule_name})

