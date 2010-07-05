set(kmodule_name "fsim_indicator_manager")

kbuild_use_symbols(
	"${CMAKE_BINARY_DIR}/base/Module.symvers"
	"${CMAKE_BINARY_DIR}/syscall_connector/kernel_part/Module.symvers"
	"${CMAKE_BINARY_DIR}/fault_simulation/base/Module.symvers")

kbuild_add_module(${kmodule_name}
	"fsim_indicator_manager_module.c"
	"module_weak_ref.c")

rule_copy_file("module_weak_ref.c" "${CMAKE_SOURCE_DIR}/module_weak_ref/module_weak_ref.c")

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})
