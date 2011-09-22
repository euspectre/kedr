# Name of the module to be created
set(kmodule_name "kedr_fsim_mem_util")

# The header of the data file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"kstrdup"
	"kstrndup"
	"kmemdup"
	
	OPTIONAL "posix_acl_alloc"
	"posix_acl_clone"
	"posix_acl_from_mode"
)

create_payload_fsim(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_fsim_add_payload(${kmodule_name})
