#name of module created
set(kmodule_name "kedr_cm_user_space_access")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kmodule_configure_kernel_functions(copy_to_user_FUNCTION REQUIRED
	ONE_OF_BEGIN "copy_to_user" "_copy_to_user" ONE_OF_END
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_to_user.data.in
	"${CMAKE_CURRENT_BINARY_DIR}/${copy_to_user_FUNCTION}.data")

kmodule_configure_kernel_functions(copy_from_user_FUNCTION REQUIRED
	ONE_OF_BEGIN "copy_from_user" "_copy_from_user" ONE_OF_END
)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_from_user.data.in
	"${CMAKE_CURRENT_BINARY_DIR}/${copy_from_user_FUNCTION}.data")

kmodule_configure_kernel_functions(strndup_user_FUNCTION REQUIRED 
	"strndup_user"
)

kmodule_configure_kernel_functions(memdup_user_FUNCTION REQUIRED 
	"memdup_user"
)

set(functions 
	${copy_to_user_FUNCTION} 
	${copy_from_user_FUNCTION} 
	${strndup_user_FUNCTION}
	${memdup_user_FUNCTION}
)

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c")

rules_generate_payload()

rules_build_datafile(${functions})

# Because copy_*_user functions require pointer to user-space data,
# common trigger target module is not applicable for trigger this functions.
# Special trigger module will be built in the
# controller/tests/call_interception directory.

# Here we all other jobs.
to_abs_path(triggers_include_file "${triggers_include_filename}")
file(READ "${triggers_include_file}" triggers_include_code)
file(APPEND "${function_intercepts_file}" "include =>>\n")
file(APPEND "${function_intercepts_file}" "${triggers_include_code}" "\n")
file(APPEND "${function_intercepts_file}" "<<\n")

configure_file("${CMAKE_SOURCE_DIR}/controller/tests/call_interception/test_user_space_access.sh.in"
	"${CMAKE_BINARY_DIR}/controller/tests/call_interception/test_user_space_access.sh"
	@ONLY)
foreach(function ${functions})
	to_abs_path(data_file_abs "${function}.data")
	file(READ "${data_file_abs}" signature_data)
	file(APPEND "${function_intercepts_file}" "${signature_data}" "\n")

	kedr_test_add_script("controller.call_interception.${function}.01"
			"${CMAKE_BINARY_DIR}/controller/tests/call_interception/test_user_space_access.sh"
			"${function}"
	)
endforeach(function ${ARGN})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})