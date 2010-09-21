kbuild_add_module("module_weak_ref_test_a"
    "module_a.c" "module_weak_ref.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/module_weak_ref.c"
 "${CMAKE_CURRENT_SOURCE_DIR}/../../module_weak_ref.c")

kedr_test_add_target(module_weak_ref_test_a)