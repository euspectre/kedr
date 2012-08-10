#name of module created
set(kmodule_name "kedr_cm_waitqueue")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

# __init_waitqueue_head()
set(checking_message "Checking the signature of __init_waitqueue_head()")
message(STATUS "${checking_message}")
if(DEFINED INIT_WAITQUEUE_HEAD_SUFFIX)
	set(checking_message "${checking_message} [cached] - done")
else(DEFINED INIT_WAITQUEUE_HEAD_SUFFIX)
	kmodule_try_compile(is___init_waitqueue_head_name # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/__init_waitqueue_head_name" # bindir
		"__init_waitqueue_head_signature.c" # srcfile
		COMPILE_DEFINITIONS "-DINIT_WAITQUEUE_HEAD_NAME"
		OUTPUT_VARIABLE out_name)

	if(is___init_waitqueue_head_name)
		set(checking_message "${checking_message} - done")
		set(INIT_WAITQUEUE_HEAD_SUFFIX "name" CACHE INTERNAL
			"The suffix for __init_waitqueue_head() data file.")
	else(is___init_waitqueue_head_name)
		kmodule_try_compile(is___init_waitqueue_head_old # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/__init_waitqueue_head_old" # bindir
			"__init_waitqueue_head_signature.c" # srcfile
			COMPILE_DEFINITIONS "-DINIT_WAITQUEUE_HEAD_OLD"
			OUTPUT_VARIABLE out_old)
		if(is___init_waitqueue_head_old)
			set(checking_message "${checking_message} - done")
			set(INIT_WAITQUEUE_HEAD_SUFFIX "old" CACHE INTERNAL
			"The suffix for __init_waitqueue_head() data file.")
		else(is___init_waitqueue_head_old)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unknown signature of __init_waitqueue_head().")
			message("Output of test compilation for __init_waitqueue_head(&q, &key):")
			message("${out_old}")
			message("Output of test compilation for __init_waitqueue_head(&q, name, &key)")
			message("${out_name}")
			message(FATAL_ERROR 
				"Failed to determine the signature of __init_waitqueue_head()")
		endif(is___init_waitqueue_head_old)
	endif(is___init_waitqueue_head_name)
endif(DEFINED INIT_WAITQUEUE_HEAD_SUFFIX)
message(STATUS "${checking_message}")

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

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/__init_waitqueue_head.data"
	"${CMAKE_CURRENT_SOURCE_DIR}/__init_waitqueue_head.${INIT_WAITQUEUE_HEAD_SUFFIX}.data")

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})

kedr_test_add_subdirectory(tests)
