#name of module created
set(kmodule_name "kedr_fsim_indicator_kmalloc")

kbuild_add_module(${kmodule_name} "indicator.c" "calculator.c" "control_file.c"
	"kedr_stack_trace.c")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_stack_trace.c"
	"${CMAKE_SOURCE_DIR}/kedr_stack_trace/kedr_stack_trace.c")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
	"${CMAKE_SOURCE_DIR}/calculator/calculator.c")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
	"${CMAKE_SOURCE_DIR}/control_file/control_file.c")


rules_generate_indicator()

kedr_install_kmodule(${kmodule_name})

