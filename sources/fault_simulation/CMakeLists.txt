set(module_name kedr_fault_simulation)

kbuild_add_module(${module_name} "fault_simulation_module.c" "control_file.c")

rule_copy_file("control_file.c"
    "${CMAKE_SOURCE_DIR}/control_file/control_file.c")

kedr_install_kmodule(${module_name})
kedr_install_symvers(${module_name})

