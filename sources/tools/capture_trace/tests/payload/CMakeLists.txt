set(module_name kedr_payload)

kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")
kbuild_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
kbuild_add_module(${module_name} "payload.c")

kedr_test_add_target(${module_name})