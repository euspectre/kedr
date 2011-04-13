# Name of the module to be created
set(kmodule_name "kedr_cm_mem_util")

# The header of the data file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"kstrdup"
	"kstrndup"
	"kmemdup"
)

create_payload_callm(${kmodule_name} ${functions})

kedr_test_add_ci_test(${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})
