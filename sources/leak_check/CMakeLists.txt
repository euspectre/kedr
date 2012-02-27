# Configuration files: for installation and for testing
set(leak_check_conf_file "leak_check.conf")
set(leak_check_test_conf_file "leak_check_test.conf")
set(leak_check_install_dir ${KEDR_INSTALL_PREFIX_KMODULE})
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${leak_check_conf_file}.in"
	"${CMAKE_CURRENT_BINARY_DIR}/${leak_check_conf_file}"
	@ONLY)
########################################################################

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${leak_check_conf_file}"
	DESTINATION "${KEDR_DEFAULT_CONFIG_DIR}") 
########################################################################

######### Common stuff for the payload modules for LeakCheck ###########
# Check if kfree_rcu() is present and set HAVE_KFREE_RCU accordingly
check_kfree_rcu()

# Common name of the payload source file
set(payload_c_file "payload.c")
# Common name of the data file
set(payload_data_file "payload.data")
# Common name of the "function support" source file
set(functions_support_file "functions_support.c")
# Common name of header part of the data file
set(header_data_file "header.data")

# Commands to create a payload module for LeakCheck.
#
# ${header_data_file} file should contain the data common for all functions 
# (i.e. #include directives).
# For each function in ${functions} list, file ${function}.data
# should contain information about the given function including the code to
# be executed before and/or after it.
function(create_payload_leak_check module_name functions)
	# Rules to build the module
	kbuild_add_module(${module_name}
		"payload.c"
		"functions_support.c")
		
	# Rules to obtain the sources of the payload and functions_support parts
	to_abs_path(payload_data_file_abs ${payload_data_file})
	add_custom_command(OUTPUT "${payload_c_file}"
		COMMAND ${KEDR_GEN_TOOL} 
			${KEDR_GEN_TEMPLATES_DIR}/payload_leak_check.c/ 
			${payload_data_file_abs} > ${payload_c_file}
		DEPENDS ${payload_data_file_abs})
	add_custom_command(OUTPUT "${functions_support_file}"
		COMMAND ${KEDR_GEN_TOOL} 
			${KEDR_GEN_TEMPLATES_DIR}/functions_support.c/ 
			${payload_data_file_abs} > ${functions_support_file}
		DEPENDS ${payload_data_file_abs})
		
	# Rules to prepare the full data file for the payload module
	set(functions_data)
	foreach(func ${functions} ${ARGN})
		list(APPEND functions_data "${func}.data")
	endforeach(func ${functions} ${ARGN})
	
	to_abs_path(source_files_abs ${header_data_file} ${functions_data})
	set(payload_data_file_abs "${CMAKE_CURRENT_BINARY_DIR}/${payload_data_file}")
	add_custom_command(OUTPUT ${payload_data_file_abs}
		COMMAND cat ${source_files_abs} > ${payload_data_file_abs}
		DEPENDS ${source_files_abs})
endfunction(create_payload_leak_check module_name functions)
########################################################################

add_subdirectory(core)
add_subdirectory(common_mm)
########################################################################

kedr_test_add_subdirectory(tests)
########################################################################
