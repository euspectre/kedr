set(module_name kedr_payload)

kbuild_add_module(${module_name}
	"payload.c"
	"functions_support.c")

kbuild_link_module(${module_name} kedr kedr_trace)

kedr_generate("functions_support.c" "functions.data"
	${KEDR_GEN_TEMPLATES_DIR}/functions_support.c)

kedr_test_install_module(${module_name})