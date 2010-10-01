kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../../Module.symvers")
kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

kbuild_add_module("fsim_test_module_a" "module_a.c")

kedr_test_add_target("fsim_test_module_a")