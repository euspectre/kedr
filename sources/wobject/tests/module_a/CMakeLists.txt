kbuild_add_module("module_wobject_a"
    "module_a.c" "wobject.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/wobject.c"
 "${CMAKE_CURRENT_SOURCE_DIR}/../../wobject.c")

kedr_test_add_target(module_wobject_a)