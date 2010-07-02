#name of module created
set(kmodule_name "kedr_cm_shedule")
#name of data file
set(kmodule_data_file "payload.data")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kedr_configure_data_pieces(functions_data REQUIRED
	"schedule"
	OPTIONAL "preempt_schedule" REQUIRED
	"_cond_resched"
	"schedule_timeout"
	"schedule_timeout_uninterruptible"
	"schedule_timeout_interruptible"
	"io_schedule"
)
kedr_configure_data_pieces(cond_resched_lock_data REQUIRED
	ONE_OF_BEGIN "cond_resched_lock" "__cond_resched_lock" ONE_OF_END
)

string(REGEX REPLACE "\\.data$" "" cond_resched_lock_FUNCTION ${cond_resched_lock_data})
configure_file("cond_resched_lock.data.in"
			"${CMAKE_CURRENT_BINARY_DIR}/${cond_resched_lock_data}")

list(APPEND functions_data ${cond_resched_lock_data})

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload(${CMAKE_CURRENT_BINARY_DIR}/${kmodule_data_file}
						${CMAKE_CURRENT_BINARY_DIR})

rules_build_datafile(${kmodule_data_file} "beginning.data"
	${functions_data})

kedr_install_kmodule(${kmodule_name})

kedr_add_default_cm_payload(${kmodule_name})