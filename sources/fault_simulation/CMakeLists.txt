#subdirs(base)
#subdirs(indicator_manager)

set(module_name kedr_fault_simulation)

kbuild_add_module(${module_name} "fault_simulation_module.c" "wobject.c")

rule_copy_file("wobject.c"
    "${CMAKE_SOURCE_DIR}/wobject/wobject.c")

add_subdirectory(tests)