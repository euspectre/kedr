# Payload modules.
if(USER_PART)
	# The only interest for USER_PART in that directory is to
	# call kedr_conf_lc_add_payload() for every payload module installed.
	function(kedr_conf_lc_add_payload module_name)
		kedr_module_ref(module_ref ${module_name})
		set_property(GLOBAL APPEND PROPERTY LC_PAYLOADS ${module_ref})
	endfunction(kedr_conf_lc_add_payload)
	# Use function immidiately.
	kedr_conf_lc_add_payload(kedr_lc_common_mm)
endif(USER_PART)

# The rest is for kernel part only
if(NOT KERNEL_PART)
	return()
endif(NOT KERNEL_PART)

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
