set(module_name kedr_payload)

kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/trace/Module.symvers")

kbuild_add_module(${module_name} "payload.c")

kedr_test_add_target(${module_name})