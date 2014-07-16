# The tests from this sub-tree check that the calls to the particular 
# functions can be intercepted by KEDR.
#
# Note that the tests are not necessarily provided for all the 
# kernel functions KEDR can process. Some functions could be easier to
# intercept than to prepare the correct tests for.
# 
# Among the functions the tests are provided for, only the calls to the 
# functions actually used by one or more payload modules are to be tested.

# kedr_create_ci_payload_data(payload_data_file func1 [func2 ...])
# Prepare the main .data file (${payload_data_file}) for the payload module 
# to be used for testing from the data files and the header data files for 
# the functions. 
#
# [!] Only the functions that are actually used by one or more payload
# modules provided with KEDR must be passed here.
function(kedr_create_ci_payload_data payload_data_file functions)
	set(header_impl_file "header_impl.data")
	set(functions_data)

	foreach(func ${functions} ${ARGN})
		kedr_get_data_for_func(${func} data_file)
		list(APPEND functions_data "${data_file}")
	endforeach(func ${functions} ${ARGN})
	
	set(hlist_data)
	kedr_get_header_data_list(hlist_data ${functions} ${ARGN})
	to_abs_path(hdata_files_abs ${hlist_data})
	add_custom_command(OUTPUT ${header_impl_file}
		COMMAND cat ${hdata_files_abs} > ${header_impl_file}
		DEPENDS ${hdata_files_abs}
	)
	
	to_abs_path(payload_data_file_abs ${payload_data_file})
	to_abs_path(source_files_abs ${header_impl_file} ${functions_data})
	
	set(payload_data_file_abs "${payload_data_file}")
	add_custom_command(OUTPUT ${payload_data_file_abs}
		COMMAND cat ${source_files_abs} > ${payload_data_file_abs}
		DEPENDS ${source_files_abs}
	)
endfunction(kedr_create_ci_payload_data payload_data_file functions)

# Install common scripts for testing purposes.

# Add a test to check if KEDR is able to intercept the calls to the given 
# functions. Only the functions that are actually used by one or more 
# payload modules provided with KEDR will be processed here, other functions
# will be ignored. If there are no used functions among the specified ones,
# the function does nothing.
#
# For each given function, ${function}.trigger data file should contain
# the following information:
# function.name - name of function to trigger
# trigger.code - the code that should trigger a call to this function.
#
# Additionally, triggers_header.data file should contain common 
# definitions for all the triggers (as parameter 'header').
#
# kedr_test_add_ci_test(function_desc_dir name function ...)
function(kedr_test_add_ci_test name functions)
	set(used_functions)
	set(have_used_functions "no")
	foreach(func ${functions} ${ARGN})
		if (DEFINED KEDR_FUNC_USED_${func})
			set(have_used_functions "yes")
			list(APPEND used_functions ${func})
		endif (DEFINED KEDR_FUNC_USED_${func})
	endforeach(func ${functions} ${ARGN})
	
	if (have_used_functions)
		set(common_dir "${CMAKE_SOURCE_DIR}/tests/global/call_int/test_common")
		
		set(trigger_name "${name}")
		# Add a test for each function
		foreach(func ${used_functions})
			kedr_test_add_script_shared("kedr.call_int.${func}.01"
				"trigger_test/test_${trigger_name}.sh"
				"${func}"
			)
		endforeach(func ${used_functions})
		
		set(trigger_data_file "${CMAKE_CURRENT_BINARY_DIR}/trigger.data")
		
		# Creating the test script
		add_subdirectory("${common_dir}/trigger_test"
			"${CMAKE_CURRENT_BINARY_DIR}/trigger_test")
		
		# Creating the payload module
		add_subdirectory("${common_dir}/trigger_payload"
			"${CMAKE_CURRENT_BINARY_DIR}/trigger_payload")
		
		# Creating the target module
		add_subdirectory("${common_dir}/trigger_target"
			"${CMAKE_CURRENT_BINARY_DIR}/trigger_target")

		itesting_path(this_install_dir)
		# Creating the data file for the target module
		configure_file("${common_dir}/trigger_common_info.data.in"
			"${CMAKE_CURRENT_BINARY_DIR}/trigger_common_info.data"
			@ONLY)
		set(trigger_data_pieces 
			"${CMAKE_CURRENT_BINARY_DIR}/trigger_common_info.data")

		set(hlist_data)
		kedr_get_header_data_list(hlist_data ${used_functions})
				
		to_abs_path(trigger_data_include "triggers_header.data")
		list(APPEND trigger_data_pieces "${trigger_data_include}"
			${hlist_data})

		foreach(function ${used_functions})
			to_abs_path(trigger_data_piece "${function}.trigger")
			list(APPEND trigger_data_pieces "${trigger_data_piece}")
		endforeach(function ${used_functions})
		
		add_custom_target(trigger_data_file_${trigger_name} ALL
			DEPENDS "${trigger_data_file}")

		add_custom_command(OUTPUT "${trigger_data_file}"
			COMMAND cat ${trigger_data_pieces} > ${trigger_data_file}
			DEPENDS ${trigger_data_pieces}
		)
		# Add dependency between the trigger data file and the target
		add_dependencies(trigger_target_${trigger_name} 
			trigger_data_file_${trigger_name})
			
		# Add dependency between the trigger data file and trigger_test
		add_dependencies(trigger_test_${trigger_name} 
			trigger_data_file_${trigger_name})
	endif (have_used_functions)
endfunction(kedr_test_add_ci_test functions)

# Function groups
add_subdirectory(capable)
add_subdirectory(common_mm)
add_subdirectory(mem_util)
add_subdirectory(mutexes)
add_subdirectory(schedule)
add_subdirectory(spinlocks)
add_subdirectory(uaccess)
add_subdirectory(virtual_mm)
add_subdirectory(waitqueue)
