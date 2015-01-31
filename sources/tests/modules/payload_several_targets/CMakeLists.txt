# Simple payload, which support several targets.
#
# Used in several places.
set(KMODULE_NAME "test_payload_several_targets")

kbuild_add_module(${KMODULE_NAME} 
    "payload.c"
    "functions_support.c"
)
kbuild_link_module(${KMODULE_NAME} kedr)

kedr_generate("functions_support.c" "functions.data"
    "${KEDR_GEN_TEMPLATES_DIR}/functions_support.c")

kedr_test_install_module (${KMODULE_NAME})
