# Name of the module to create
set(kmodule_name "kedr_sample_indicator_test_module")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
kbuild_add_module(${kmodule_name} "module.c")

kedr_test_add_target(${kmodule_name})