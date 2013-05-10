set(kedr_test_conf_dir "${CMAKE_CURRENT_BINARY_DIR}/conf")
set(kedr_test_conf_file_default "${kedr_test_conf_dir}/default.conf")

file(MAKE_DIRECTORY "${kedr_test_conf_dir}")

# Configure kedr script
kedr_load_test_prefixes()
set(KEDR_DEFAULT_CONFIG_DIR "${kedr_test_conf_dir}")
set(KEDR_DEFAULT_CONFIG_FILE "${kedr_test_conf_file_default}")

set(KEDR_FILE "${CMAKE_BINARY_DIR}/core/kedr.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../kedr.in
                ${CMAKE_CURRENT_BINARY_DIR}/kedr
                @ONLY)

# Create predefined configuration files
if(KEDR_STANDARD_CALLM_PAYLOADS)
    # Default config file is same as the call monitor configuration file
    add_custom_target(kedr_test_config_file_default ALL
                    DEPENDS "${kedr_test_conf_file_default}")

    kedr_generate("default.conf" "${kedr_test_conf_file_data_callm}"
        "${templates_dir}/kedr_callm.conf")
    
    rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/default.conf"
        "${kedr_test_conf_file_default}")

    kedr_test_add_target("kedr_test_config_file_default")
else(KEDR_STANDARD_CALLM_PAYLOADS)
    # Default config file will be empty
    FILE(WRITE "${kedr_test_conf_file_default}" "#Empty configuration file\n")
endif(KEDR_STANDARD_CALLM_PAYLOADS)


if(KEDR_STANDARD_CALLM_PAYLOADS)
set(NAME_OF_PAYLOAD_LOADED "kedr_cm_cmm")
else(KEDR_STANDARD_CALLM_PAYLOADS)
set(NAME_OF_PAYLOAD_LOADED)
endif(KEDR_STANDARD_CALLM_PAYLOADS)

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
                "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
                @ONLY)

kedr_test_add_script("control_service.01" "test.sh")