# Name of the module to create
set(kmodule_name "kedr_fsim_uaccess")

if(USER_PART)
	kedr_conf_fsim_add_payload(${kmodule_name})
endif(USER_PART)

# The rest is for kernel part only.
if(NOT KERNEL_PART)
	return()
endif(NOT KERNEL_PART)

# The header part of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

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

set(functions 
	${copy_to_user_FUNCTION} 
	${copy_from_user_FUNCTION} 
	${strndup_user_FUNCTION}
	${memdup_user_FUNCTION}
)

create_payload_fsim(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})
