# Additional symbols to use. Note that the symbols from KEDR Core are
# made available to the payload modules automatically if 
# kedr_create_payload_module() is used to create them.
# The dependency on KEDR Core is also set automatically for such modules.
kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
kbuild_add_dependencies("kedr_fault_simulation")

# The names of the main data file and of the file containing the 
# payload-specific portion of its header.
set(payload_data_file "payload.data")
set(header_data_file "header.data")

# The commands to create a payload module for 
function(create_payload_fsim module_name functions)
	set(payload_data_file_impl ${payload_data_file})
	
	if (KEDR_ENABLE_CALLER_ADDRESS)
		set(payload_data_file_impl 
			"${CMAKE_CURRENT_BINARY_DIR}/payload_with_caller_address.data")
		
		add_custom_command(OUTPUT "${payload_data_file_impl}"
			COMMAND ${KEDR_GEN_TOOL} 
			"${KEDR_GEN_TEMPLATES_DIR}/payload_fsim_add_caller_address.data/" 
			${payload_data_file} > "${payload_data_file_impl}"
			DEPENDS ${payload_data_file})
	endif (KEDR_ENABLE_CALLER_ADDRESS)
	
	kedr_create_payload_module(${module_name} ${payload_data_file_impl} 
		"${KEDR_GEN_TEMPLATES_DIR}/payload_fsim.c/")
		
	# Rules to prepare the full data file for the payload module 
	# (at this stage, without support for 'caller address' variable).
	kedr_create_payload_data(${header_data_file} ${payload_data_file}
		${functions} ${ARGN})
endfunction(create_payload_fsim module_name functions)

add_subdirectory(common_mm)
add_subdirectory(uaccess)
add_subdirectory(capable)
add_subdirectory(mem_util)
add_subdirectory(virtual_mm)

kedr_test_add_subdirectory(tests)