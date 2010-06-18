#name of module created
set(kmodule_name "kedr_cm_waitqueue")
#name of data file
set(kmodule_data_file "payload.data")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kedr_configure_data_pieces(functions_data REQUIRED
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
	"payload.c"
	"trace_payload.h")

rules_generate_payload(${CMAKE_CURRENT_BINARY_DIR}/${kmodule_data_file}
						${CMAKE_CURRENT_BINARY_DIR})

rules_build_datafile(${kmodule_data_file} "beginning.data"
	${functions_data})

kedr_install_kmodule(${kmodule_name})