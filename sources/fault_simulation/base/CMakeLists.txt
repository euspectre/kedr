kbuild_add_module("fsim_base"
	"fsim_base_module.c"
	"${CMAKE_BINARY_DIR}/module_weak_ref/module_weak_ref.o")
rule_copy_source("fsim_base_module.c")