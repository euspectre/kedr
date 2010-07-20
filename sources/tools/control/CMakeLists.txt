set(kedr_conf_filename "kedr.conf")
set(kedr_conf_file "${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_filename}")
set(kedr_test_conf_filename "kedr_test.conf")
set(kedr_test_conf_file "${CMAKE_CURRENT_BINARY_DIR}/${kedr_test_conf_filename}")

#Create empty config files
file(WRITE ${kedr_cm_conf_file})
file(WRITE ${kedr_test_cm_conf_file})

#By default, content ${kedr_cm_conf_file} is copied into ${kedr_conf_file}.
add_custom_target("kedr_conf" ALL
                    DEPENDS ${kedr_conf_file})

rule_copy_file(${kedr_conf_file} ${kedr_cm_conf_file})
#Same for config file for test
add_custom_target("kedr_test_conf"
                    DEPENDS ${kedr_test_conf_file})

rule_copy_file(${kedr_test_conf_file} ${kedr_test_cm_conf_file})

kedr_test_add_target(kedr_test_conf)


#for configure kedr.sh
set(DEFAULT_KEDR_CONF_FILE "${KEDR_INSTALL_PREFIX_VAR}/${kedr_conf_filename}")

set(BASE_MODULE "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_base.ko")
set(CONTROLLER_MODULE "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_controller.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kedr.in
                ${CMAKE_CURRENT_BINARY_DIR}/kedr
                @ONLY)

#same for configure kedr_test.sh(for tests)
set(DEFAULT_KEDR_CONF_FILE "${CMAKE_CURRENT_BINARY_DIR}/${kedr_test_conf_filename}")

set(BASE_MODULE "${CMAKE_BINARY_DIR}/base/kedr_base.ko")
set(CONTROLLER_MODULE "${CMAKE_BINARY_DIR}/controller/kedr_controller.ko")

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