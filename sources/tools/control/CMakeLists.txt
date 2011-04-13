set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

# Paths to default config file, for install variant we need also name of the file
set(kedr_conf_file_default_name "default.conf")
set(kedr_conf_file_default "${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_default_name}")

if(KEDR_STANDARD_CALLM_PAYLOADS)
    # Write header of call monitor data-file
    file(WRITE "${kedr_conf_file_data_callm}"
    	"kedr_trace_module = ${KEDR_INSTALL_PREFIX_KMODULE}/kedr_trace.ko\n"
    )
    if(NOT CMAKE_CROSSCOMPILING)
        # Same but for test variant
        file(WRITE "${kedr_test_conf_file_data_callm}"
    	    "kedr_trace_module = ${CMAKE_BINARY_DIR}/trace/kedr_trace.ko\n"
           )
    endif(NOT CMAKE_CROSSCOMPILING)

    # Function for add loading of given payload into
    # call monitor configuration file.
    #
    # Also, test variant for this configuration file is created.
    # For make it correct, payload module should be located in the current
    # binary directory, from where the function is invoked.
    function(kedr_conf_callm_add_payload payload_name)
        file(APPEND ${kedr_conf_file_data_callm} "payload.name = ${payload_name}\n")
    	file(APPEND ${kedr_conf_file_data_callm} "payload.path = ${KEDR_INSTALL_PREFIX_KMODULE}/${payload_name}.ko\n")
        if(NOT CMAKE_CROSSCOMPILING)
        	file(APPEND ${kedr_test_conf_file_data_callm} "payload.name = ${payload_name}\n")
        	file(APPEND ${kedr_test_conf_file_data_callm} "payload.path = ${CMAKE_CURRENT_BINARY_DIR}/${payload_name}.ko\n")
        endif(NOT CMAKE_CROSSCOMPILING)
    endfunction(kedr_conf_callm_add_payload payload_name)

endif(KEDR_STANDARD_CALLM_PAYLOADS)
if(KEDR_STANDARD_FSIM_PAYLOADS)
    file(WRITE "${kedr_conf_file_data_fsim}"
    	"fault_simulation_module = ${KEDR_INSTALL_PREFIX_KMODULE}/kedr_fault_simulation.ko\n"
    )
    
    # Function for add loading of given payload into
    # fault simulation configuration file.
    function(kedr_conf_fsim_add_payload payload_name)
	    file(APPEND ${kedr_conf_file_data_fsim} "payload.name = ${payload_name}\n")
		file(APPEND ${kedr_conf_file_data_fsim} "payload.path = ${KEDR_INSTALL_PREFIX_KMODULE}/${payload_name}.ko\n")
    endfunction(kedr_conf_fsim_add_payload payload_name)

endif(KEDR_STANDARD_FSIM_PAYLOADS)

# Configure kedr script(install mode)
kedr_load_install_prefixes()
set(KEDR_DEFAULT_CONFIG_FILE "${KEDR_DEFAULT_CONFIG_DIR}/${kedr_conf_file_default_name}")

set(KEDR_FILE "${KEDR_INSTALL_PREFIX_KMODULE}/kedr.ko")

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
    # Build configuration file for fault simulation payloads
    set(kedr_conf_file_fsim "${CMAKE_CURRENT_BINARY_DIR}/fsim.conf")
	
	add_custom_target(kedr_config_file_fsim ALL
                    DEPENDS "${kedr_conf_file_fsim}")

	add_custom_command(OUTPUT "${kedr_conf_file_fsim}"
					COMMAND "${KEDR_GEN_TOOL}" "${templates_dir}/kedr_fsim.conf" "${kedr_conf_file_data_fsim}" > "${kedr_conf_file_fsim}"
                    DEPENDS "${kedr_conf_file_data_fsim}")

    install(FILES "${kedr_conf_file_fsim}"
			DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}")
endif(KEDR_STANDARD_FSIM_PAYLOADS)

if(KEDR_STANDARD_CALLM_PAYLOADS)
    # Build configuration file for call monitor payloads
    set(kedr_conf_file_callm "${CMAKE_CURRENT_BINARY_DIR}/callm.conf")

	add_custom_target(kedr_config_file_callm ALL
                    DEPENDS "${kedr_conf_file_callm}")

	add_custom_command(OUTPUT "${kedr_conf_file_callm}"
					COMMAND "${KEDR_GEN_TOOL}" "${templates_dir}/kedr_callm.conf" "${kedr_conf_file_data_callm}" > "${kedr_conf_file_callm}"
                    DEPENDS "${kedr_conf_file_data_callm}")

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


kedr_test_add_subdirectory(tests)
