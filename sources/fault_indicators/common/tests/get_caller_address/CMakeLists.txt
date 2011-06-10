set(kmodule_name "get_caller_address")

set(functions_support_data "${CMAKE_CURRENT_SOURCE_DIR}/functions_support.data")

kbuild_add_module(${kmodule_name} "payload.c" "functions_support.c")

add_custom_command(OUTPUT "functions_support.c"
            COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ ${functions_support_data} > functions_support.c
            DEPENDS ${functions_support_data})

kedr_test_add_target(${kmodule_name})