set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

# Paths to default config files(for install and test modes)
set(kedr_test_conf_file_default "${CMAKE_CURRENT_BINARY_DIR}/default_test.conf")
# For install variant we need also name of the file
set(kedr_conf_file_default_name "default.conf")
set(kedr_conf_file_default "${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_default_name}")

# Paths to header of the data-files(for install and test modes)
set(kedr_conf_file_data_header "${CMAKE_CURRENT_BINARY_DIR}/kedr_conf_header.data")
set(kedr_test_conf_file_data_header "${CMAKE_CURRENT_BINARY_DIR}/kedr_test_conf_header.data")

# Write headers of data-files
file(WRITE "${kedr_conf_file_data_header}"
	"fault_simulation_module = ${KEDR_INSTALL_PREFIX_KMODULE}/kedr_fault_simulation.ko\n"
	"kedr_trace_module = ${KEDR_INSTALL_PREFIX_KMODULE}/kedr_trace.ko\n"
)

file(WRITE "${kedr_test_conf_file_data_header}"
	"fault_simulation_module = ${CMAKE_BINARY_DIR}/fault_simulation/kedr_fault_simulation.ko\n"
	"kedr_trace_module = ${CMAKE_BINARY_DIR}/trace/kedr_trace.ko\n"
)

#Function for create config file from pieces
function(create_config_file target_name file_name datafile_header_name datafile_name)
SET(tmp_filename "./${target_name}_tmp.data")
add_custom_target("${target_name}" ALL
                    DEPENDS "${file_name}")

add_custom_command(OUTPUT "${file_name}"
					COMMAND cat "${datafile_header_name}" "${datafile_name}" > "${tmp_filename}"
					COMMAND "${KEDR_GEN_TOOL}" "${templates_dir}/kedr.conf" "${tmp_filename}" > "${file_name}"
					COMMAND rm "${tmp_filename}"
                    DEPENDS "${datafile_header_name}" "${datafile_name}")
endfunction(create_config_file target_name file_name datafile_header_name datafile_name)

# Configure kedr script(install mode)
kedr_load_install_prefixes()
set(KEDR_DEFAULT_CONFIG_FILE "${KEDR_DEFAULT_CONFIG_DIR}/${kedr_conf_file_default_name}")

set(KEDR_FILE_BASE "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_base.ko")
set(KEDR_FILE_CONTROLLER "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_controller.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kedr.in
                ${CMAKE_CURRENT_BINARY_DIR}/kedr
                @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr"
			DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
            PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)

# Create predefined configuration files(install mode)
install(FILES "${kedr_conf_file_default}"
		DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}")

if(KEDR_STANDARD_FSIM_PAYLOADS)
    # Path to fault simulation config file
    set(kedr_conf_file_fsim "${CMAKE_CURRENT_BINARY_DIR}/fsim.conf")

    create_config_file(kedr_config_file_fsim
        "${kedr_conf_file_fsim}" "${kedr_conf_file_data_header}" "${kedr_conf_file_data_fsim}")
    install(FILES "${kedr_conf_file_fsim}"
			DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}")
endif(KEDR_STANDARD_FSIM_PAYLOADS)

if(KEDR_STANDARD_CALLM_PAYLOADS)
    # Path to call monitor config file
    set(kedr_conf_file_callm "${CMAKE_CURRENT_BINARY_DIR}/callm.conf")

    create_config_file(kedr_config_file_callm
        "${kedr_conf_file_callm}" "${kedr_conf_file_data_header}" "${kedr_conf_file_data_callm}")
    install(FILES "${kedr_conf_file_callm}"
			DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}")
    
    # Default config file will be copy of this one
    add_custom_target(kedr_config_file_default ALL
                    DEPENDS "${kedr_conf_file_default}")
    rule_copy_file("${kedr_conf_file_default}" "${kedr_conf_file_callm}")
    # Both targets kedr_config_file_default and kedr_config_file_callm depends from one file
    # ${kedr_conf_file_callm}. For do not allow them to create this file concurrently,
    # force some ordering between these targets.
    add_dependencies(kedr_config_file_default kedr_config_file_callm)
else(KEDR_STANDARD_CALLM_PAYLOADS)
    # Default config file will be empty
    FILE(WRITE "${kedr_conf_file_default}" "#Empty configuration file\n")
endif(KEDR_STANDARD_CALLM_PAYLOADS)

# Configure kedr script(test mode)
kedr_load_test_prefixes()
set(KEDR_DEFAULT_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(KEDR_DEFAULT_CONFIG_FILE "${kedr_test_conf_file_default}")

set(KEDR_FILE_BASE "${CMAKE_BINARY_DIR}/base/kedr_base.ko")
set(KEDR_FILE_CONTROLLER "${CMAKE_BINARY_DIR}/controller/kedr_controller.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kedr.in
                ${CMAKE_CURRENT_BINARY_DIR}/kedr_test
                @ONLY)

# Create predefined configuration files(test mode)
if(KEDR_STANDARD_CALLM_PAYLOADS)
    # Default config file will be copy of the call monitor configuration file
    create_config_file(kedr_test_config_file_default
    	"${kedr_test_conf_file_default}" "${kedr_test_conf_file_data_header}" "${kedr_test_conf_file_data_callm}")
else(KEDR_STANDARD_CALLM_PAYLOADS)
    # Default config file will be empty - copy of its install variant
    add_custom_target(kedr_test_config_file_default ALL
                        DEPENDS "${kedr_test_conf_file_default}")
    rule_copy_file("${kedr_test_conf_file_default}" "${kedr_conf_file_default}")
endif(KEDR_STANDARD_CALLM_PAYLOADS)

kedr_test_add_target("kedr_test_config_file_default")

kedr_test_add_subdirectory(tests)
