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

# The names of the main data file and of the file containing the 
# payload-specific portion of its header.
set(payload_data_file "payload.data")
set(header_data_file "header.data")

# The commands to create a payload module for LeakCheck.
function(create_payload_leak_check module_name functions)
	kedr_create_payload_module(${module_name} ${payload_data_file} 
		"${KEDR_GEN_TEMPLATES_DIR}/payload_leak_check.c/")
	# Link payload with leak check module.
	kbuild_link_module(${module_name} kedr_leak_check)
	# Rules to prepare the full data file for the payload module
	kedr_create_payload_data(${header_data_file} ${payload_data_file}
		${functions} ${ARGN})
endfunction(create_payload_leak_check module_name functions)
########################################################################

add_subdirectory(core)
add_subdirectory(common_mm)
