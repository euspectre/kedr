kbuild_use_symbols(${CMAKE_CURRENT_BINARY_DIR}/../Module.symvers)
kbuild_add_module(${test_module_name}
    "test_module.c")

kedr_test_add_target(${test_module_name})