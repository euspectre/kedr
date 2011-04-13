set(kmodule_name trigger_payload)

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_module(${kmodule_name} 
	payload.c
	functions_support.c)

add_custom_command(OUTPUT "payload.c"
	COMMAND ${KEDR_GEN_TOOL} "${templates_dir}/trigger_payload.c" ${function_intercepts_file} > payload.c
	DEPENDS ${function_intercepts_file})

add_custom_command(OUTPUT "functions_support.c"
	COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ ${function_intercepts_file} > functions_support.c
	DEPENDS "${function_intercepts_file}")

kedr_test_add_target(${kmodule_name})
