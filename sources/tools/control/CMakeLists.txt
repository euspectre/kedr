set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

# Paths to default config file, for install variant we need also name of the file
set(kedr_conf_file_default_name "default.conf")
set(kedr_conf_file_default "${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_default_name}")

if (KEDR_STANDARD_CALLM_PAYLOADS)
	# Write header of call monitor data-file
	file(WRITE "${kedr_conf_file_data_callm}"
		"kedr_trace_module = ${KEDR_INSTALL_PREFIX_KMODULE}/kedr_trace.ko\n"
	)
	if (NOT CMAKE_CROSSCOMPILING)
		# Same but for test variant
		file(WRITE "${kedr_test_conf_file_data_callm}"
			"kedr_trace_module = ${CMAKE_BINARY_DIR}/trace/kedr_trace.ko\n"
		   )
	endif (NOT CMAKE_CROSSCOMPILING)

	# The function adds the instructions to load the given payload module 
	# in the configuration file for call monitoring.
	#
	# Besides that, a test variant of this configuration file is created.
	# To make it correct, the payload module should be located in the current
	# binary directory, the directory the function is invoked from.
	function(kedr_conf_callm_add_payload payload_name)
		file(APPEND ${kedr_conf_file_data_callm} "payload.name = ${payload_name}\n")
		file(APPEND ${kedr_conf_file_data_callm} "payload.path = ${KEDR_INSTALL_PREFIX_KMODULE}/${payload_name}.ko\n")
		if (NOT CMAKE_CROSSCOMPILING)
			file(APPEND ${kedr_test_conf_file_data_callm} "payload.name = ${payload_name}\n")
			file(APPEND ${kedr_test_conf_file_data_callm} "payload.path = ${CMAKE_CURRENT_BINARY_DIR}/${payload_name}.ko\n")
		endif (NOT CMAKE_CROSSCOMPILING)
	endfunction(kedr_conf_callm_add_payload payload_name)

endif (KEDR_STANDARD_CALLM_PAYLOADS)
if (KEDR_STANDARD_FSIM_PAYLOADS)
	file(WRITE "${kedr_conf_file_data_fsim}"
		"fault_simulation_module = ${KEDR_INSTALL_PREFIX_KMODULE}/kedr_fault_simulation.ko\n"
	)
	
	# The function adds the instructions to load the given payload module 
	# in the configuration file for fault simulation.
	function(kedr_conf_fsim_add_payload payload_name)
		file(APPEND "${kedr_conf_file_data_fsim}" 
			"payload.name = ${payload_name}\n")
		file(APPEND "${kedr_conf_file_data_fsim}" 
			"payload.path = ${KEDR_INSTALL_PREFIX_KMODULE}/${payload_name}.ko\n")
	endfunction(kedr_conf_fsim_add_payload payload_name)
	
	# The function adds the instructions to load the given indicator 
	# module in the configuration file for fault simulation.
	function(kedr_conf_fsim_add_indicator indicator_module_name)
		file(APPEND "${kedr_conf_file_data_fsim}" 
			"indicator.path = ${KEDR_INSTALL_PREFIX_KMODULE}/${indicator_module_name}.ko\n")
	endfunction(kedr_conf_fsim_add_indicator indicator_module_name)
endif(KEDR_STANDARD_FSIM_PAYLOADS)

# Configure KEDR control script.
set(KEDR_DEFAULT_CONFIG_FILE 
	"${KEDR_DEFAULT_CONFIG_DIR}/${kedr_conf_file_default_name}")

set(KEDR_FILE "${KEDR_INSTALL_PREFIX_KMODULE}/kedr.ko")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kedr.in
				${CMAKE_CURRENT_BINARY_DIR}/kedr
				@ONLY)

install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/kedr"
	DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
)

# Create predefined configuration files(install mode)
install(FILES "${kedr_conf_file_default}"
		DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}")

if (KEDR_STANDARD_FSIM_PAYLOADS)
	# Build configuration file for fault simulation payloads
	set(kedr_conf_file_fsim "${CMAKE_CURRENT_BINARY_DIR}/fsim.conf")
	
	add_custom_target(kedr_config_file_fsim ALL
					DEPENDS "${kedr_conf_file_fsim}")

	kedr_generate("fsim.conf" "${kedr_conf_file_data_fsim}"
		"${templates_dir}/kedr_fsim.conf")
	
	install(FILES "${kedr_conf_file_fsim}"
			DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}")
endif (KEDR_STANDARD_FSIM_PAYLOADS)

if (KEDR_STANDARD_CALLM_PAYLOADS)
	# Build configuration file for call monitor payloads
	set(kedr_conf_file_callm "${CMAKE_CURRENT_BINARY_DIR}/callm.conf")

	add_custom_target(kedr_config_file_callm ALL
					DEPENDS "${kedr_conf_file_callm}")

	kedr_generate("callm.conf" "${kedr_conf_file_data_callm}"
		"${templates_dir}/kedr_callm.conf")

	install(FILES "${kedr_conf_file_callm}"
			DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}")
endif (KEDR_STANDARD_CALLM_PAYLOADS)

# By default, LeakCheck should be used, if enabled.
if (KEDR_LEAK_CHECK)    
	set(kedr_config_file_leak_check 
		"${CMAKE_BINARY_DIR}/leak_check/leak_check.conf")
	
	# Default config file will be a copy of the config file for LeakCheck
	add_custom_target(kedr_config_file_default ALL
		DEPENDS "${kedr_conf_file_default}")
	rule_copy_file(
		"${kedr_conf_file_default}" 
		"${kedr_config_file_leak_check}")
else ()
	# Default config file will be empty
	FILE(WRITE "${kedr_conf_file_default}" "# Empty configuration file\n")
endif (KEDR_LEAK_CHECK)
