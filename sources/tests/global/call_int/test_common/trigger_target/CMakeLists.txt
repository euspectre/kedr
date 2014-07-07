# ${trigger_name} - the name of the trigger (used as a suffix for the 
# module to be created).
# ${trigger_data_file} - the .data file for the target.

# Because this directory is included using 2-arguments add_subdirectory()
# command, we need to notify ictest about that.
ictest_add_current_dir()

set(trigger_target_templates_dir 
	"${CMAKE_CURRENT_SOURCE_DIR}/templates_trigger_target.c")

kbuild_add_module(trigger_target_${trigger_name}
    "${CMAKE_CURRENT_BINARY_DIR}/target.c"
)
kedr_test_install_module(trigger_target_${trigger_name})

kedr_generate("target.c" "${trigger_data_file}" "${trigger_target_templates_dir}")