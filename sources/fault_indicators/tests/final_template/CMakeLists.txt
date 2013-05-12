kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")

# Common name of data file
set(indicator_data_file "indicator.data")

set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/fault_indicators/final_template")

# Commands for generate indicator.c and place it into current directory
# from ${indicator_data_file}, placed in the current source or binary directory.
function(rules_generate_internal_indicator)
	kedr_generate("indicator.c" "${indicator_data_file}"
		"${KEDR_GEN_TEMPLATES_DIR}/fault_indicator.c")
endfunction(rules_generate_internal_indicator)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_common.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_common.sh"
	@ONLY)

add_subdirectory(module)
add_subdirectory(simulate_never)
add_subdirectory(simulate_always)
add_subdirectory(fail_with_even_instances)
add_subdirectory(state_variable)
add_subdirectory(with_ro_file)
add_subdirectory(with_file)
add_subdirectory(with_parameter)
add_subdirectory(cascade_simulate)
add_subdirectory(cascade_simulate_complex)