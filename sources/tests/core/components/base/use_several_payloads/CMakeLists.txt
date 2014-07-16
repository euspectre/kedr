set(base_module_name "base_module_use_several_payloads_m")
set(payload_module1_name "base_test_payload_module1")
set(payload_module2_name "base_test_payload_module2")
set(payload_module3_name "base_test_payload_module3")
set(verificator_module_name "base_verificator_module")

itesting_path(this_install_dir)


rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_base.c"
    "${CORE_SOURCE_DIR}/kedr_base.c"
)

kbuild_add_module(${base_module_name}
    "base_module.c"
    "kedr_base.c"
)

kedr_test_install_module(${base_module_name})

add_subdirectory(payload1)
add_subdirectory(payload2)
add_subdirectory(payload3)
add_subdirectory(interception_info_verificator)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test1.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test1.sh"
    @ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test2.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test2.sh"
    @ONLY
)

kedr_test_add_script("core.components.base.use_several_payloads.01"
    "test1.sh")

kedr_test_add_script("core.components.base.use_several_payloads.02"
    "test2.sh")
