#  example_create_build_script(imported_example_target script_file)
#
# Create shell script, which copy example files to temporary directory
# and build it there. Directory for copy and build example
# may be passed to the script itself. By default, 'build' directory is used.
#
# 'imported_example_target' should be IMPORTED target for example,
# which contains install locations of the example.
#
# Created script may be used as simple test for example or be called
# from other test script.
function(example_create_build_script imported_example_target script_file)
	get_property(is_example TARGET ${imported_example_target} SET PROPERTY EXAMPLE_TYPE)
	if(NOT is_example)
		message(FATAL_ERROR "Target is not example target: ${imported_example_target}")
	endif(NOT is_example)

	get_property(is_imported TARGET ${imported_example_target} PROPERTY EXAMPLE_IMPORTED)
	if(NOT is_imported)
		message(FATAL_ERROR "Example target is not imported: ${imported_example_target}")
	endif(NOT is_imported)

	get_property(example_location TARGET ${imported_example_target} PROPERTY EXAMPLE_IMPORTED_LOCATION)
	configure_file("${CMAKE_SOURCE_DIR}/tests/examples/build_example.sh.in"
		"${script_file}")
endfunction(example_create_build_script)


add_subdirectory(counters)
add_subdirectory(custom_indicator_fsim)

if(KEDR_CALLM_PAYLOADS)
	add_subdirectory(custom_payload_callm)
endif(KEDR_CALLM_PAYLOADS)

add_subdirectory(custom_payload_fsim)

if(KEDR_LEAK_CHECK)
	add_subdirectory(leak_check)
endif(KEDR_LEAK_CHECK)

add_subdirectory(sample_fsim_payload)
add_subdirectory(sample_indicator)

add_subdirectory(sample_fsim)
