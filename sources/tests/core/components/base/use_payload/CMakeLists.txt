set(base_module_name "base_module_use_payload_m")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_base.c"
    "${CORE_SOURCE_DIR}/kedr_base.c"
)

kbuild_add_module(${base_module_name}
    "base_module_use_payload.c"
    "kedr_base.c"
)

kedr_test_install_module(${base_module_name})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

kedr_test_add_script("core.components.base.01"
    "test.sh"
)

