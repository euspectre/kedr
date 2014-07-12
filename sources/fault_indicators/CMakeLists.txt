if(USER_PART)
	# The only interest for USER_PART in that directory is to
	# call kedr_conf_fsim_add_indicator() for every module installed.
	function(kedr_conf_fsim_add_indicator module_name)
		kedr_module_ref(module_ref ${module_name})
		set_property(GLOBAL APPEND PROPERTY FSIM_INDICATORS ${module_ref})
	endfunction(kedr_conf_fsim_add_indicator)
endif(USER_PART)
if(KERNEL_PART)
	# Common name of data file
	set(indicator_data_file "indicator.data")

	# Commands for create indicator module from
	# ${indicator_data_file}, placed in the current source or binary directory.
	function(create_indicator module_name)
		kbuild_add_module(${module_name} 
			"indicator.c" 
			"calculator.c" 
			"control_file.c")

		kbuild_link_module(${module_name} kedr kedr_fault_simulation)
		
		rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
			"${CMAKE_SOURCE_DIR}/calculator/calculator.c")

		rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
			"${CMAKE_SOURCE_DIR}/control_file/control_file.c")

		kedr_generate("indicator.c" "indicator_internal.data"
			"${KEDR_GEN_TEMPLATES_DIR}/fault_indicator.c")
		
		if(KEDR_ENABLE_CALLER_ADDRESS)
			kedr_generate("indicator_internal.data" "indicator_with_caller_address.data"
				"${KEDR_GEN_TEMPLATES_DIR}/fault_indicator.data")
			kedr_generate("indicator_with_caller_address.data" "${indicator_data_file}"
				"${KEDR_GEN_TEMPLATES_DIR}/fault_indicator_add_caller_address.data")
		else(KEDR_ENABLE_CALLER_ADDRESS)
			kedr_generate("indicator_internal.data" "${indicator_data_file}"
				"${KEDR_GEN_TEMPLATES_DIR}/fault_indicator.data")
		endif(KEDR_ENABLE_CALLER_ADDRESS)
	endfunction(create_indicator module_name)
endif(KERNEL_PART)

add_subdirectory(kmalloc)
add_subdirectory(capable)
add_subdirectory(common)


