#name of module created
set(kmodule_name "kedr_cm_schedule")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"schedule"
	OPTIONAL "preempt_schedule" REQUIRED
	"_cond_resched"
	"schedule_timeout"
	"schedule_timeout_uninterruptible"
	"schedule_timeout_interruptible"
	"io_schedule"
)

kmodule_configure_kernel_functions(cond_resched_lock_FUNCTION REQUIRED
	ONE_OF_BEGIN "cond_resched_lock" "__cond_resched_lock" ONE_OF_END
)
configure_file("cond_resched_lock.data.in"
			"${CMAKE_CURRENT_BINARY_DIR}/${cond_resched_lock_FUNCTION}.data")

list(APPEND functions ${cond_resched_lock_FUNCTION})

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})

kedr_test_add_subdirectory(tests)
