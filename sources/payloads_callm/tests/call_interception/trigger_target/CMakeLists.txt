# trigger_name should contrain name of trigger(used as suffix for module created)
# trigger_data_file should contain datafile for target.

set(trigger_target_templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates_trigger_target.c")

kbuild_add_module(trigger_target_${trigger_name}
    "${CMAKE_CURRENT_BINARY_DIR}/target.c"
)
kedr_test_add_target(trigger_target_${trigger_name})

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/target.c"
    COMMAND ${KEDR_GEN_TOOL} ${trigger_target_templates_dir} ${trigger_data_file} > ${CMAKE_CURRENT_BINARY_DIR}/target.c
    DEPENDS ${trigger_data_file}
)