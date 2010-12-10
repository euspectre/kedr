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

set(functions ${copy_to_user_FUNCTION} ${copy_from_user_FUNCTION})

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload()

rules_build_datafile(${functions})

kedr_test_add_triggers_include()
kedr_test_add_trigger_for_function(
	"${CMAKE_CURRENT_BINARY_DIR}/${kmodule_name}.ko"
	${functions}
)

kedr_install_kmodule(${kmodule_name})

kedr_conf_default_add_payload(${kmodule_name})