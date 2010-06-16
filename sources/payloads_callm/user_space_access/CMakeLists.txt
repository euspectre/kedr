#name of module created
set(kmodule_name "kedr_cm_user_space_access")
#name of data file
set(kmodule_data_file "payload.data")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kedr_configure_data_pieces(copy_to_user_data REQUIRED
	ONE_OF_BEGIN "copy_to_user" "_copy_to_user" ONE_OF_END
)

string(REGEX REPLACE "\\.data$" "" copy_to_user_FUNCTION ${copy_to_user_data})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_to_user.data.in
	${CMAKE_CURRENT_BINARY_DIR}/${copy_to_user_data})

kedr_configure_data_pieces(copy_from_user_data REQUIRED
	ONE_OF_BEGIN "copy_from_user" "_copy_from_user" ONE_OF_END
)
string(REGEX REPLACE "\\.data$" "" copy_from_user_FUNCTION ${copy_from_user_data})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_from_user.data.in
	${CMAKE_CURRENT_BINARY_DIR}/${copy_from_user_data})

set(functions_data ${copy_to_user_data} ${copy_from_user_data})

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload(${CMAKE_CURRENT_BINARY_DIR}/${kmodule_data_file}
						${CMAKE_CURRENT_BINARY_DIR})

rules_build_datafile(${kmodule_data_file} "beginning.data"
	${functions_data})

kedr_install_kmodule(${kmodule_name})