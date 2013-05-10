set(kmodule_name "payload_fsim_fpoint_with_params")

kedr_create_payload_module(${kmodule_name} "payload.data"
    "${KEDR_GEN_TEMPLATES_DIR}/payload_fsim.c")

kedr_test_add_target(${kmodule_name})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY)

kedr_test_add_script("payloads_fsim.final_template.fpoint_with_params.01"
    "test.sh")