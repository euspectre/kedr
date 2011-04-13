set(module_name kedr_payload)

set(functions_data_file "${CMAKE_CURRENT_SOURCE_DIR}/functions.data")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/trace/Module.symvers")

kbuild_add_module(${module_name}
	"payload.c"
	"functions_support.c")

add_custom_command(OUTPUT "functions_support.c"
	COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ ${functions_data_file} > functions_support.c
	DEPENDS "${functions_data_file}")

kedr_test_add_target(${module_name})