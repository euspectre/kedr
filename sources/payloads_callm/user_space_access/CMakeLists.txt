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

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})

kedr_test_add_subdirectory(tests)