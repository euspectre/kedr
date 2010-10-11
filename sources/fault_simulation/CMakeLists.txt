set(module_name kedr_fault_simulation)

kbuild_add_module(${module_name} "fault_simulation_module.c" "wobject.c")

rule_copy_file("wobject.c"
    "${CMAKE_SOURCE_DIR}/wobject/wobject.c")

kedr_install_kmodule(${module_name})
kedr_install_symvers(${module_name})

add_subdirectory(tests)
#Only for tests
add_subdirectory(calculator)

