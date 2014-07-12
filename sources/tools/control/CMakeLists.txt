# Configuration for call monitoring
if (KEDR_STANDARD_CALLM_PAYLOADS)
	# Form content of call monitoring config file.
	set(callm_conf_content
"#Load kedr tracing module
module ${KEDR_TRACE_REF}
#Load call monitor payloads
"
)
	
	get_property(callm_payloads GLOBAL PROPERTY CALLM_PAYLOADS)
	foreach(payload ${callm_payloads})
		set(callm_conf_content "${callm_conf_content}payload ${payload}\n")
	endforeach(payload ${callm_payloads})
	
	set(kedr_conf_file_callm_name "callm.conf")
	
	file_update(${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_callm_name}
		"${callm_conf_content}")
	
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_callm_name}
		DESTINATION ${KEDR_DEFAULT_CONFIG_DIR}
	)
endif (KEDR_STANDARD_CALLM_PAYLOADS)

# Configuration for fault simulation
if (KEDR_STANDARD_FSIM_PAYLOADS)
	# Form content of fault simulation config file.
	set(fsim_conf_content
"# Load the fault simulation subsystem
module ${KEDR_FAULT_SIMULATION_REF}
# Load the standard fault simulation payloads
"
)
	
	get_property(fsim_payloads GLOBAL PROPERTY FSIM_PAYLOADS)
	foreach(payload ${fsim_payloads})
		set(fsim_conf_content "${fsim_conf_content}payload ${payload}\n")
	endforeach(payload ${fsim_payloads})
	
	set(fsim_conf_content "${fsim_conf_content}# Load the standard indicators\n")

	get_property(fsim_indicators GLOBAL PROPERTY FSIM_INDICATORS)
	foreach(indicator ${fsim_indicators})
		set(fsim_conf_content "${fsim_conf_content}module ${indicator}\n")
	endforeach(indicator ${fsim_indicators})

	
	set(kedr_conf_file_fsim_name "fsim.conf")

	file_update(${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_fsim_name}
		"${fsim_conf_content}")
	
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_fsim_name}
		DESTINATION ${KEDR_DEFAULT_CONFIG_DIR}
	)
endif(KEDR_STANDARD_FSIM_PAYLOADS)

# Configuration for memory leak checker.
if(KEDR_LEAK_CHECK)
	# Form content of leak check config file.
	set(lc_conf_content
"# Memory leak detector (LeakCheck), the core and the plugins
module ${KEDR_LEAK_CHECK_REF}
"
)
	
	get_property(lc_payloads GLOBAL PROPERTY LC_PAYLOADS)
	foreach(payload ${lc_payloads})
		set(lc_conf_content "${lc_conf_content}payload ${payload}\n")
	endforeach(payload ${lc_payloads})
	
	set(kedr_conf_file_lc_name "leak_check.conf")
	
	file_update(${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_lc_name}
		"${lc_conf_content}")
	
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_lc_name}
		DESTINATION ${KEDR_DEFAULT_CONFIG_DIR}
	)

endif(KEDR_LEAK_CHECK)
# Default config file.
set(kedr_conf_file_default_name "default.conf")
# Choose default configuration
if(KEDR_LEAK_CHECK)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_lc_name}
		DESTINATION ${KEDR_DEFAULT_CONFIG_DIR}
		RENAME ${kedr_conf_file_default_name}
	)
else(KEDR_LEAK_CHECK)
	# Default config file will be empty
	file_update(${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_default_name}
		"# Empty configuration file\n"
	)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${kedr_conf_file_default_name}
		DESTINATION ${KEDR_DEFAULT_CONFIG_DIR}
	)
endif(KEDR_LEAK_CHECK)

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
