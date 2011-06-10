set(kmodule_name "payload_fsim_rename_fpoint")

set(payload_data "${CMAKE_CURRENT_SOURCE_DIR}/payload.data")

kbuild_add_module(${kmodule_name} "payload.c" "functions_support.c")

add_custom_command(OUTPUT "payload.c"
            COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/payload_fsim.c/ ${payload_data} > payload.c
            DEPENDS ${payload_data})

add_custom_command(OUTPUT "functions_support.c"
            COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ ${payload_data} > functions_support.c
            DEPENDS ${payload_data})

kedr_test_add_target(${kmodule_name})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY)

kedr_test_add_script("payloads_fsim.final_template.rename_fpoint.01"
    "test.sh")