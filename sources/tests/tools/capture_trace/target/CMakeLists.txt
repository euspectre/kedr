set(module_name kedr_target)

kbuild_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
kbuild_add_module(${module_name} "target.c")

kedr_test_install_module(${module_name})