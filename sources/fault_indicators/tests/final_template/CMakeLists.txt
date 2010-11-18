kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")

# Common name of data file
set(indicator_data_file "indicator.data")

# Commands for generate indicator.c and place it into current directory
# from ${indicator_data_file}, placed in the current source or binary directory.
function(rules_generate_internal_indicator)
	set(indicator_c_file "${CMAKE_CURRENT_BINARY_DIR}/indicator.c")
	to_abs_path(indicator_data_file_abs ${indicator_data_file})
	add_custom_command(OUTPUT ${indicator_c_file}
				COMMAND ${KEDR_GEN_TOOL} ${KEDR_GEN_TEMPLATES_DIR}/indicator.c/ ${indicator_data_file_abs} > ${indicator_c_file}
				DEPENDS ${indicator_data_file_abs})
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