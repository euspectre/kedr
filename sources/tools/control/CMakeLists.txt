set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

# data file for config file in test mode(produced from install data file)
set(kedr_test_cm_conf_file_data "${CMAKE_CURRENT_BINARY_DIR}/kedr_test_cm.conf.data")
# Directory for local mount debugfs for enable payloads tracepoints
#(should be used with KEDR_PREFIX_TEMP_SESSION prefix)
set(kedr_debugfs_dir "debugfs")

# Paths to call monitor config files(for install and test modes)
set(kedr_cm_conf_file "${CMAKE_CURRENT_BINARY_DIR}/kedr_cm.conf")
set(kedr_test_cm_conf_file "${CMAKE_CURRENT_BINARY_DIR}/kedr_test_cm.conf")

# Paths to default call monitor config files(for install and test modes)
# We also need a name of install version of file for reference to file when it is installed(in control script)
set(kedr_conf_filename "kedr.conf")
set(kedr_conf_file "${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_filename}")
set(kedr_test_conf_file "${CMAKE_CURRENT_BINARY_DIR}/kedr_test.conf")

# Start to writting config data-file
file(WRITE ${kedr_cm_conf_file_data}
    "debugfs_install_mount_point = ${KEDR_INSTALL_PREFIX_TEMP_SESSION}/${kedr_debugfs_dir}\n"
    "debugfs_test_mount_point = ${KEDR_TEST_PREFIX_TEMP_SESSION}/${kedr_debugfs_dir}\n"
)

# How to create call monitor config file
add_custom_target(kedr_control_cm_config_file ALL
                    DEPENDS "${kedr_cm_conf_file}")

add_custom_command(OUTPUT "${kedr_cm_conf_file}"
                    COMMAND "${KEDR_GEN_TOOL}" "${templates_dir}/kedr_cm.conf" "${kedr_cm_conf_file_data}" > "${kedr_cm_conf_file}"
                    DEPENDS "${kedr_cm_conf_file_data}")

# How to create its variant for tests
add_custom_target(kedr_test_control_cm_config_file
                    DEPENDS "${kedr_test_cm_conf_file}")

add_custom_command(OUTPUT "${kedr_test_cm_conf_file}"
                    COMMAND "${KEDR_GEN_TOOL}" "${templates_dir}/kedr_cm.conf" "${kedr_test_cm_conf_file_data}" > "${kedr_test_cm_conf_file}"
                    DEPENDS "${kedr_test_cm_conf_file_data}")

add_custom_command(OUTPUT "${kedr_test_cm_conf_file_data}"
                    COMMAND echo "is_test = 1" > "${kedr_test_cm_conf_file_data}"
                    COMMAND cat "${kedr_cm_conf_file_data}" >> "${kedr_test_cm_conf_file_data}"
                    DEPENDS "${kedr_cm_conf_file_data}")

kedr_test_add_target(kedr_test_control_cm_config_file)

#By default, content ${kedr_cm_conf_file} is copied into ${kedr_conf_file}.
add_custom_target(kedr_control_config_file ALL
                    DEPENDS "${kedr_conf_file}")

rule_copy_file("${kedr_conf_file}" "${kedr_cm_conf_file}")
add_dependencies(kedr_control_config_file kedr_control_cm_config_file)
#Same for config file for test
add_custom_target(kedr_test_control_config_file
                    DEPENDS ${kedr_test_conf_file})

rule_copy_file("${kedr_test_conf_file}" "${kedr_test_cm_conf_file}")

add_dependencies(kedr_test_control_config_file kedr_test_control_cm_config_file)
kedr_test_add_target(kedr_test_control_config_file)


#for configure kedr.sh
kedr_load_install_prefixes()
set(KEDR_FILE_DEFAULT_CONTROL_CONFIG "${KEDR_INSTALL_PREFIX_VAR}/${kedr_conf_filename}")

set(KEDR_FILE_BASE "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_base.ko")
set(KEDR_FILE_CONTROLLER "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_controller.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kedr.in
                ${CMAKE_CURRENT_BINARY_DIR}/kedr
                @ONLY)

#same for configure kedr_test.sh(for tests)
kedr_load_test_prefixes()
set(KEDR_FILE_DEFAULT_CONTROL_CONFIG "${kedr_test_conf_file}")

set(KEDR_FILE_BASE "${CMAKE_BINARY_DIR}/base/kedr_base.ko")
set(KEDR_FILE_CONTROLLER "${CMAKE_BINARY_DIR}/controller/kedr_controller.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kedr.in
                ${CMAKE_CURRENT_BINARY_DIR}/kedr_test
                @ONLY)

add_subdirectory(tests)

install(FILES "${kedr_cm_conf_file}" "${kedr_conf_file}"
			DESTINATION "${KEDR_INSTALL_PREFIX_VAR}"
            )

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr"
			DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
            PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)