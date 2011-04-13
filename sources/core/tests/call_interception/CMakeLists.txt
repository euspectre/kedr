#  Add call interception test for the given functions.
#
# 'triggers_include.list' should contain includes for functions.
#
# For each function 'f' its trigger code should be in the file 'f.trigger'
# and its signature should be contained in 'f.data'.
# kedr_test_add_ci_test(function ...)
#
# This functions is used in payloads_callm/
function(kedr_test_add_ci_test functions)
	# Add include directive for triggers
	to_abs_path(triggers_include_file "triggers_include.list")
	file(READ "${triggers_include_file}" triggers_include_code)
	file(APPEND "${function_triggers_file}" "header =>>\n")
	file(APPEND "${function_triggers_file}" "${triggers_include_code}" "\n")
	file(APPEND "${function_triggers_file}" "<<\n")
	# Add the same for call interception payload
	file(APPEND "${function_intercepts_file}" "header =>>\n")
	file(APPEND "${function_intercepts_file}" "${triggers_include_code}" "\n")
	file(APPEND "${function_intercepts_file}" "<<\n")
	# Add each function
	foreach(function ${functions} ${ARGN})
		file(APPEND ${function_triggers_file} "function.name = ${function}\n")
		file(APPEND ${function_triggers_file} "trigger_code =>>\n")
		to_abs_path(trigger_file "${function}.trigger")
		file(READ "${trigger_file}" trigger_code)
		file(APPEND ${function_triggers_file} "${trigger_code}" "\n")
		file(APPEND ${function_triggers_file} "<<\n")

		to_abs_path(data_file_abs "${function}.data")
		file(READ "${data_file_abs}" signature_data)
		file(APPEND "${function_intercepts_file}" "${signature_data}" "\n")

		kedr_test_add_script("core.call_interception.${function}.01"
				"${CMAKE_BINARY_DIR}/core/tests/call_interception/test.sh"
				"${function}"
		)
	endforeach(function ${functions} ${ARGN})
endfunction(kedr_test_add_ci_test functions)

#  Add call interception test for the given functions.
# Variant of previos function, but intented to use for functions
# which use user-space access.
#
# Because of last fact, it is difficult to trigger function call in
# common way. So, trigger is hardcoded into corresponding target module.
#
# 'triggers_include.list' should contain includes for functions.
#
# For each function 'f' its signature should be contained in 'f.data'.
# kedr_test_add_ci_test_uaccess(function ...)
# This functions is used in payloads_callm/
function(kedr_test_add_ci_test_uaccess functions)
	# Add include directive for call interception payload
	to_abs_path(triggers_include_file "triggers_include.list")
	file(READ "${triggers_include_file}" triggers_include_code)
	file(APPEND "${function_intercepts_file}" "header =>>\n")
	file(APPEND "${function_intercepts_file}" "${triggers_include_code}" "\n")
	file(APPEND "${function_intercepts_file}" "<<\n")
	# Add each function
	foreach(function ${functions} ${ARGN})

		to_abs_path(data_file_abs "${function}.data")
		file(READ "${data_file_abs}" signature_data)
		file(APPEND "${function_intercepts_file}" "${signature_data}" "\n")

		kedr_test_add_script("core.call_interception.${function}.01"
				"${CMAKE_BINARY_DIR}/core/tests/call_interception/test_user_space_access.sh"
				"${function}"
		)
	endforeach(function ${functions} ${ARGN})
endfunction(kedr_test_add_ci_test_uaccess functions)


set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

add_subdirectory(trigger_payload)
add_subdirectory(trigger_target)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in
		${CMAKE_CURRENT_BINARY_DIR}/test.sh
		@ONLY)

kmodule_configure_kernel_functions(copy_to_user_FUNCTION REQUIRED
	ONE_OF_BEGIN "copy_to_user" "_copy_to_user" ONE_OF_END
)

kmodule_configure_kernel_functions(copy_from_user_FUNCTION REQUIRED
	ONE_OF_BEGIN "copy_from_user" "_copy_from_user" ONE_OF_END
)

kmodule_configure_kernel_functions(strndup_user_FUNCTION REQUIRED 
	"strndup_user"
)

kmodule_configure_kernel_functions(memdup_user_FUNCTION REQUIRED 
	"memdup_user"
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_user_space_access.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_user_space_access.sh"
	@ONLY)

#Add trigger target for user space access functions.
add_subdirectory(trigger_target_user_space_access)

# Tests themselves will be added with call monitor payloads definitions
