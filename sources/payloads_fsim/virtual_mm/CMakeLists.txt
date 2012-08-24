# Name of the module to create
set(kmodule_name "kedr_fsim_vmm")

# The header part of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

# free()-like functions are only monitored (they cannot fail anyway) 
kmodule_configure_kernel_functions(functions REQUIRED
	"vmalloc"
	"__vmalloc"
	"vmalloc_user"
	"vmalloc_node"
	"vmalloc_32"
	"vmalloc_32_user"
	OPTIONAL "vzalloc"
	"vzalloc_node"
)

create_payload_fsim(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_fsim_add_payload(${kmodule_name})
