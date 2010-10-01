set(module_name "kedr_fsim_base")

kbuild_add_module(${module_name}
	"fsim_base_module.c"
	"module_weak_ref.c"
    "wobject.c")

rule_copy_file("module_weak_ref.c" "${CMAKE_SOURCE_DIR}/module_weak_ref/module_weak_ref.c")
rule_copy_file("wobject.c" "${CMAKE_SOURCE_DIR}/wobject/wobject.c")

kedr_install_kmodule(${module_name})
kedr_install_symvers(${module_name})

add_subdirectory(tests)