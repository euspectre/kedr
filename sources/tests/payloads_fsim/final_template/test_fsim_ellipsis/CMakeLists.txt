set(kmodule_name "payload_fsim_ellipsis")

add_test_fsim_payload(${kmodule_name})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

add_test_payload_fsim("payloads_fsim.final_template.fsim_ellipsis.01"
    "test.sh"
)
