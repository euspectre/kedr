# Common name of data file
set(indicator_data_file "indicator.data")

# Create fault simulation indicator module for tests.
# ${indicator_data_file}, placed in the current source or binary
# directory, is used as data-file for indicator created.
function(add_test_indicator kmodule_name)
	kbuild_add_module(${kmodule_name} "indicator.c" "control_file.c")
	
	kbuild_link_module(${kmodule_name} kedr kedr_fault_simulation)

	rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
		"${CMAKE_SOURCE_DIR}/control_file/control_file.c")

	kedr_generate("indicator.c" "${indicator_data_file}"
		"${KEDR_GEN_TEMPLATES_DIR}/fault_indicator.c")

	kedr_test_install_module(${kmodule_name})
endfunction(add_test_indicator kmodule_name)

add_subdirectory(module)

itesting_path(this_install_dir)
set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/fault_indicators/final_template")


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_common.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_common.sh"
	@ONLY)

kedr_test_install(PROGRAMS "test_common.sh")

add_subdirectory(simulate_never)
add_subdirectory(simulate_always)
add_subdirectory(fail_with_even_instances)
add_subdirectory(state_variable)
add_subdirectory(with_ro_file)
add_subdirectory(with_file)
add_subdirectory(with_parameter)
add_subdirectory(cascade_simulate)
add_subdirectory(cascade_simulate_complex)