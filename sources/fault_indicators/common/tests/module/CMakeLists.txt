#name of module created
set(kmodule_name "kedr_indicator_common_test_module")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/module.c.in"
	"${CMAKE_CURRENT_BINARY_DIR}/module.c")

kbuild_add_module(${kmodule_name} "module.c")

kedr_test_add_target(${kmodule_name})
