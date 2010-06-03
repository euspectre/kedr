set(kmodule_name "fsim_indicator_manager")

kbuild_use_symbols(
	"${CMAKE_BINARY_DIR}/controller/Module.symvers"
	"${CMAKE_BINARY_DIR}/syscall_connector/kernel_part/Module.symvers"
	"${CMAKE_BINARY_DIR}/fault_simulation/base/Module.symvers")

kbuild_add_module(${kmodule_name}
	"fsim_indicator_manager_module.c"
	"${CMAKE_BINARY_DIR}/module_weak_ref/module_weak_ref.o")

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})
