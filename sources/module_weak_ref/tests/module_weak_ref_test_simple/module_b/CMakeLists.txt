kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../module_a/Module.symvers")
kbuild_add_module("module_weak_ref_test_simple_b" "module_b.c")

kedr_test_add_target(module_weak_ref_test_simple_b)