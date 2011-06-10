kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")

kbuild_add_module(${indicator_greater_than_module_name} indicator.c)

kedr_test_add_target(${indicator_greater_than_module_name})