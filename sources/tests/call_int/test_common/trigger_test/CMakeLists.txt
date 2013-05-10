# ${trigger_name} - the name of the trigger (used as a suffix for the file
# to be created).
# ${trigger_data_file} - the .data file needed to generate the script.

set(trigger_test_templates_dir 
	"${CMAKE_CURRENT_SOURCE_DIR}/templates_trigger_test.sh")

add_custom_target(trigger_test_${trigger_name}
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/test_${trigger_name}.sh"
)

kedr_generate("test_${trigger_name}.sh" "${trigger_data_file}"
    "${trigger_test_templates_dir}")

kedr_test_add_target(trigger_test_${trigger_name})
