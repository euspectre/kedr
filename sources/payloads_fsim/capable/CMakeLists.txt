#name of module created
set(kmodule_name "kedr_fsim_capable")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"capable"
)

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c")

rules_generate_payload()

rules_build_datafile(${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_fsim_add_payload(${kmodule_name})