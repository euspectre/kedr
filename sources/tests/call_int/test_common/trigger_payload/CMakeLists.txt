# ${trigger_name} should be the name of the trigger (used as a suffix 
# for the module to be created).
# ${trigger_payload_data_file} should be the .data file for the payload
# module.
# ${used_functions} - the list of the kernel functions to generate the 
# payload module for.

set(trigger_payload_data_file 
	"${CMAKE_CURRENT_BINARY_DIR}/payload.data")
kedr_create_ci_payload_data("${trigger_payload_data_file}"
	${used_functions}
)

set(trigger_payload_templates_dir 
	"${CMAKE_CURRENT_SOURCE_DIR}/templates_trigger_payload.c")

kedr_create_payload_module(trigger_payload_${trigger_name} 
	"${trigger_payload_data_file}"
	"${trigger_payload_templates_dir}")
	
kedr_test_add_target(trigger_payload_${trigger_name})
