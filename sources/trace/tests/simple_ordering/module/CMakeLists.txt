set(kmodule_name simple_ordering_module)

kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../../../Module.symvers")
kbuild_add_module(${kmodule_name} "module.c")

kedr_test_add_target(${kmodule_name})