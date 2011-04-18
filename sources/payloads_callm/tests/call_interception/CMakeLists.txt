#
#  Add call interception test for the given functions.
#
# This test verify(indirectly) that all given functions
# are correctly intercepts by payload from some standard code.
#
#  File ../payload.data is used as datafile, described payload.
#
# For each given function, datafile ${function}.trigger should contain
# information about trigger:
# function.name - name of function to trigger
# trigger.code - code which should trigger function call.
#
# Additionally, datafile triggers_include.list should contain common definitions
# for all triggers(as parameter 'header').
#
# kedr_test_add_ci_test(function_desc_dir name function ...)
#
# This functions is used in payloads_callm/
function(kedr_test_add_ci_test name functions)
	set(trigger_name "${name}")
    # Add test for each function
	foreach(function ${functions} ${ARGN})
		kedr_test_add_script("payloads_callm.call_interception.${function}.01"
				"${CMAKE_CURRENT_BINARY_DIR}/trigger_test/test_${trigger_name}.sh"
				"${function}"
		)
	endforeach(function ${functions} ${ARGN})
    
    set(trigger_data_file "${CMAKE_CURRENT_BINARY_DIR}/trigger.data")
    # How to make test script
    add_subdirectory("${CMAKE_SOURCE_DIR}/payloads_callm/tests/call_interception/trigger_test"
        "${CMAKE_CURRENT_BINARY_DIR}/trigger_test")
    # How to make trigger_payload from datafile(which already has built)
    set(trigger_payload_data_file "${CMAKE_CURRENT_BINARY_DIR}/../payload.data")
    add_subdirectory("${CMAKE_SOURCE_DIR}/payloads_callm/tests/call_interception/trigger_payload"
        "${CMAKE_CURRENT_BINARY_DIR}/trigger_payload")
    # How to make trigger target from datafile
    add_subdirectory("${CMAKE_SOURCE_DIR}/payloads_callm/tests/call_interception/trigger_target"
        "${CMAKE_CURRENT_BINARY_DIR}/trigger_target")

    # How to make trigger datafile
    configure_file("${CMAKE_SOURCE_DIR}/payloads_callm/tests/call_interception/trigger_common_info.data.in"
        "${CMAKE_CURRENT_BINARY_DIR}/trigger_common_info.data"
        @ONLY)
    set(trigger_data_pieces "${CMAKE_CURRENT_BINARY_DIR}/trigger_common_info.data")

    to_abs_path(trigger_data_include "triggers_include.list")
    list(APPEND trigger_data_pieces "${trigger_data_include}")

    foreach(function ${functions} ${ARGN})
        to_abs_path(trigger_data_piece "${function}.trigger")
        list(APPEND trigger_data_pieces "${trigger_data_piece}")
    endforeach(function ${functions} ${ARGN})
    
    add_custom_target(trigger_data_file_${trigger_name}
        DEPENDS "${trigger_data_file}")
    kedr_test_add_target(trigger_data_file_${trigger_name})
    add_custom_command(OUTPUT "${trigger_data_file}"
        COMMAND cat ${trigger_data_pieces} > ${trigger_data_file}
        DEPENDS ${trigger_data_pieces}
    )
    # Add dependecy between trigger datafile and trigger_target
    add_dependencies(trigger_target_${trigger_name} trigger_data_file_${trigger_name})
    # Add dependecy between trigger datafile and trigger_test
    add_dependencies(trigger_test_${trigger_name} trigger_data_file_${trigger_name})
endfunction(kedr_test_add_ci_test functions)
