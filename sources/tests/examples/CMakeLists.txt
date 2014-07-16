#  example_create_build_script(script_file example_location)
#
# Create shell script, which copy example files to temporary directory
# and build it there. Directory for copy and build example
# may be passed to the script itself.
# By default, build/ directory is used as build directory.
#
# 'example_location' should contain path where example is installed.
# Relative path is interpreted relative to KEDR_INSTALL_PREFIX_EXAMPLES.
#
# Created script may be used as simple test for the example or be called
# from other test script.
function(example_create_build_script script_file example_location)
	if(NOT IS_ABSOLUTE ${example_location})
		set(example_location ${KEDR_INSTALL_PREFIX_EXAMPLES}/${example_location})
	endif(NOT IS_ABSOLUTE ${example_location})
	configure_file("${CMAKE_SOURCE_DIR}/tests/examples/build_example.sh.in"
		"${script_file}"
	)
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
