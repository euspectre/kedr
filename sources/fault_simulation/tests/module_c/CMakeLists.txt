kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../../Module.symvers")
kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../module_a/Module.symvers")
kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

kbuild_add_module("fsim_base_test_c" "module_c.c")

kedr_test_add_target("fsim_base_test_c")