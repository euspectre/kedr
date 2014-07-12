# Name of the module to create
set(kmodule_name "kedr_cm_waitqueue")

if(USER_PART)
	kedr_conf_callm_add_payload(${kmodule_name})
endif(USER_PART)

# The rest is for kernel part only.
if(NOT KERNEL_PART)
	return()
endif(NOT KERNEL_PART)

# The header part of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"__wake_up"
	"prepare_to_wait"
	"finish_wait"
	"remove_wait_queue"
	"add_wait_queue"
	"add_wait_queue_exclusive"
	OPTIONAL "init_waitqueue_head"
	"__init_waitqueue_head"
)

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})
