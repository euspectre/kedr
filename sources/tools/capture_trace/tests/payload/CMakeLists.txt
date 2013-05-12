set(module_name kedr_payload)

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/trace/Module.symvers")

kbuild_add_module(${module_name}
	"payload.c"
	"functions_support.c")

kedr_generate("functions_support.c" "functions.data"
	${KEDR_GEN_TEMPLATES_DIR}/functions_support.c)

kedr_test_add_target(${module_name})