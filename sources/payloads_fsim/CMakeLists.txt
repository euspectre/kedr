# The names of the main data file and of the file containing the 
# payload-specific portion of its header.
set(payload_data_file "payload.data")
set(header_data_file "header.data")

# The commands to create a payload module for 
function(create_payload_fsim module_name functions)
	set(payload_data_file_impl ${payload_data_file})
	
	if (KEDR_ENABLE_CALLER_ADDRESS)
		kedr_generate("payload_with_caller_address.data" ${payload_data_file_impl}
			"${KEDR_GEN_TEMPLATES_DIR}/payload_fsim_add_caller_address.data/")
		set(payload_data_file_impl "payload_with_caller_address.data")

	endif (KEDR_ENABLE_CALLER_ADDRESS)
	
	kedr_create_payload_module(${module_name} ${payload_data_file_impl} 
		"${KEDR_GEN_TEMPLATES_DIR}/payload_fsim.c/")
	kbuild_link_module(${module_name} kedr_fault_simulation)
	
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