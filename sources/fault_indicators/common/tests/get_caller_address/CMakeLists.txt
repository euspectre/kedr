set(kmodule_name "get_caller_address")

kbuild_add_module(${kmodule_name}
    "payload.c"
    "functions_support.c"
)

kedr_generate("functions_support.c" "functions_support.data"
    "${KEDR_GEN_TEMPLATES_DIR}/functions_support.c")

kedr_test_add_target(${kmodule_name})