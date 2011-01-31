#name of module created
set(kmodule_name "kedr_cm_waitqueue")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"__wake_up"
	ONE_OF_BEGIN "init_waitqueue_head" "__init_waitqueue_head" ONE_OF_END
	"prepare_to_wait"
	"finish_wait"
	"remove_wait_queue"
	"add_wait_queue"
	"add_wait_queue_exclusive"
)

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c")

rules_generate_payload()

rules_build_datafile(${functions})

kedr_test_add_ci_include()
kedr_test_add_ci_test(${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})
kedr_conf_fsim_add_payload(${kmodule_name})