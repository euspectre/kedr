# Name of the module to create
set(kmodule_name "kedr_fsim_capable")

# The header part of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"capable"
)

create_payload_fsim(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_fsim_add_payload(${kmodule_name})
