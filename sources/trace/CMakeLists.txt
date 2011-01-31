set(kmodule_name "kedr_trace")
set(kmodule "${kmodule_name}.ko")

set(checking_message "Checking ring_buffer_consume signature")
message(STATUS "${checking_message}")
if((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
	set(checking_message "${checking_message} [cached] - done")
else((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
	kmodule_try_compile(is_ring_buffer_consume_has_4_args # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_consume_4_args" # bindir
		"ring_buffer_consume_signature.c" # srcfile
		COMPILE_DEFINITIONS "-DRING_BUFFER_CONSUME_HAS_4_ARGS"
		OUTPUT_VARIABLE out_4)

	if(is_ring_buffer_consume_has_4_args)
		set(checking_message "${checking_message} - done")
		set(RING_BUFFER_CONSUME_HAS_4_ARGS "TRUE" CACHE INTERNAL
			"Whether ring_buffer_consume accept 4 arguments")
	else(is_ring_buffer_consume_has_4_args)
		kmodule_try_compile(is_ring_buffer_consume_has_3_args # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_consume_3_args" # bindir
			"ring_buffer_consume_signature.c" # srcfile
			COMPILE_DEFINITIONS "-DRING_BUFFER_CONSUME_HAS_3_ARGS"
			OUTPUT_VARIABLE out_3)
		if(is_ring_buffer_consume_has_3_args)
			set(checking_message "${checking_message} - done")
			set(RING_BUFFER_CONSUME_HAS_3_ARGS "TRUE" CACHE INTERNAL
			"Whether ring_buffer_consume accept 3 arguments")
		else(is_ring_buffer_consume_has_3_args)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unknown signature of ring_buffer_consume.")
			message("Output of test compilation for 3 arguments:")
			message("${out_3}")
			message("Output of test compilation for 4 arguments:")
			message("${out_4}")
			message(FATAL_ERROR "")
		endif(is_ring_buffer_consume_has_3_args)
	endif(is_ring_buffer_consume_has_4_args)
endif((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
message(STATUS "${checking_message}")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${kmodule_name} 
	"kedr_trace_module.c" "trace_buffer.c" "trace_buffer.h" "trace_file.c" "trace_file.h"
)

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})

# The tests
add_subdirectory (tests)
