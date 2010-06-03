set(module_name "fsim_base")

kbuild_add_module(${module_name}
	"fsim_base_module.c"
	"${CMAKE_BINARY_DIR}/module_weak_ref/module_weak_ref.o")

kedr_install_kmodule(${module_name})
kedr_install_symvers(${module_name})