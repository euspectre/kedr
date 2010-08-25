kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/syscall_connector/kernel_part/Module.symvers")
kbuild_add_module("test_use_named_libraries_service_test_module" "test_module.c")

kedr_test_add_target("test_use_named_libraries_service_test_module")