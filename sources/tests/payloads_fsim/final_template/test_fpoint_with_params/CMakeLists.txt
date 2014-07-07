set(kmodule_name "payload_fsim_fpoint_with_params")

add_test_fsim_payload(${kmodule_name})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY)

kedr_test_add_script("payloads_fsim.final_template.fpoint_with_params.01"
    "test.sh")