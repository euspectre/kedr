# trigger_name should contrain name of trigger(used as suffix for module created)
# trigger_payload_data_file should contain datafile for payload.

set(trigger_payload_templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates_trigger_payload.c")

kbuild_add_module(trigger_payload_${trigger_name}
    "${CMAKE_CURRENT_BINARY_DIR}/payload.c"
    "${CMAKE_CURRENT_BINARY_DIR}/functions_support.c")
kedr_test_add_target(trigger_payload_${trigger_name})

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/payload.c"
    COMMAND ${KEDR_GEN_TOOL} ${trigger_payload_templates_dir} ${trigger_payload_data_file} > ${CMAKE_CURRENT_BINARY_DIR}/payload.c
    DEPENDS ${trigger_payload_data_file}
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/functions_support.c"
	COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ ${trigger_payload_data_file} > ${CMAKE_CURRENT_BINARY_DIR}/functions_support.c
	DEPENDS "${trigger_payload_data_file}")