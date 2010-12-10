set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

# data file for config file in test mode(produced from install data file)
set(kedr_test_cm_conf_file_data "${CMAKE_CURRENT_BINARY_DIR}/kedr_test_cm.conf.data")
# Directory for local mount debugfs for enable payloads tracepoints
#(should be used with KEDR_PREFIX_TEMP_SESSION prefix)
set(kedr_debugfs_dir "debugfs")

# Paths to default config files(for install and test modes)
set(kedr_test_conf_file_default "${CMAKE_CURRENT_BINARY_DIR}/kedr_test.conf")
# For install variant we need also name of the file
set(kedr_conf_file_default_name "kedr.conf")
set(kedr_conf_file_default "${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_default_name}")

# Create data-file for default config file
file(WRITE "${kedr_conf_file_data_default}" "")
file(WRITE "${kedr_test_conf_file_data_default}" "")

if(KEDR_STANDARD_FSIM_PAYLOADS)
# Path to fault simulation config file(install mode only)
set(kedr_conf_file_fsim "${CMAKE_CURRENT_BINARY_DIR}/kedr_fsim.conf")
# Create data-file for fault simulation config file
file(WRITE "${kedr_conf_file_data_fsim}" "")
endif(KEDR_STANDARD_FSIM_PAYLOADS)

# Paths to header of the data-files(for install and test modes)
set(kedr_conf_file_data_header "${CMAKE_CURRENT_BINARY_DIR}/kedr_conf_header.data")
set(kedr_test_conf_file_data_header "${CMAKE_CURRENT_BINARY_DIR}/kedr_test_conf_header.data")

# Write headers of data-files
file(WRITE "${kedr_conf_file_data_header}"
	"debugfs_mount_point = ${KEDR_INSTALL_PREFIX_TEMP_SESSION}/${kedr_debugfs_dir}\n"
	"fault_simulation_module = ${KEDR_INSTALL_PREFIX_KMODULE}/kedr_fault_simulation.ko\n"
)

file(WRITE "${kedr_test_conf_file_data_header}"
	"debugfs_mount_point = ${KEDR_TEST_PREFIX_TEMP_SESSION}/${kedr_debugfs_dir}\n"
	"fault_simulation_module = ${CMAKE_BINARY_DIR}/fault_simulation/kedr_fault_simulation.ko\n"
)

#Function for create config file
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

# How to create default config file
create_config_file(kedr_control_config_file_default
	"${kedr_conf_file_default}" "${kedr_conf_file_data_header}" "${kedr_conf_file_data_default}")
# How to create its test variant for tests
create_config_file(kedr_test_control_config_file_default
	"${kedr_test_conf_file_default}" "${kedr_test_conf_file_data_header}" "${kedr_test_conf_file_data_default}")

if(KEDR_STANDARD_FSIM_PAYLOADS)
# How to create config file for fault simulation
create_config_file(kedr_control_config_file_fsim
	"${kedr_conf_file_fsim}" "${kedr_conf_file_data_header}" "${kedr_conf_file_data_fsim}")
endif(KEDR_STANDARD_FSIM_PAYLOADS)

kedr_test_add_target(kedr_test_control_config_file_default)

#for configure kedr script
kedr_load_install_prefixes()
set(KEDR_FILE_DEFAULT_CONTROL_CONFIG "${KEDR_INSTALL_PREFIX_VAR}/${kedr_conf_file_default_name}")

set(KEDR_FILE_BASE "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_base.ko")
set(KEDR_FILE_CONTROLLER "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_controller.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kedr.in
                ${CMAKE_CURRENT_BINARY_DIR}/kedr
                @ONLY)

#same for configure kedr_test script(for tests)
kedr_load_test_prefixes()
set(KEDR_FILE_DEFAULT_CONTROL_CONFIG "${kedr_test_conf_file_default}")

set(KEDR_FILE_BASE "${CMAKE_BINARY_DIR}/base/kedr_base.ko")
set(KEDR_FILE_CONTROLLER "${CMAKE_BINARY_DIR}/controller/kedr_controller.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kedr.in
                ${CMAKE_CURRENT_BINARY_DIR}/kedr_test
                @ONLY)

add_subdirectory(tests)

install(FILES "${kedr_conf_file_default}"
			DESTINATION "${KEDR_INSTALL_PREFIX_VAR}"
            )

if(KEDR_STANDARD_FSIM_PAYLOADS)
install(FILES "${kedr_conf_file_fsim}"
			DESTINATION "${KEDR_INSTALL_PREFIX_VAR}"
            )
endif(KEDR_STANDARD_FSIM_PAYLOADS)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr"
			DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
            PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)