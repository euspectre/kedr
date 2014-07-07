# ${trigger_name} - the name of the trigger (used as a suffix for the file
# to be created).
# ${trigger_data_file} - the .data file needed to generate the script.

# Because this directory is included using 2-arguments add_subdirectory()
# command, we need to notify ictest about that.
ictest_add_current_dir()


set(trigger_test_templates_dir 
	"${CMAKE_CURRENT_SOURCE_DIR}/templates_trigger_test.sh")

add_custom_target(trigger_test_${trigger_name} ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/test_${trigger_name}.sh"
)

kedr_generate("test_${trigger_name}.sh" "${trigger_data_file}"
    "${trigger_test_templates_dir}")

kedr_test_install(PROGRAMS "test_${trigger_name}.sh")
