set(kmodule_name "kedr_trace")
set(kmodule "${kmodule_name}.ko")

# ring_buffer_consume
set(checking_message "Checking the signature of ring_buffer_consume()")
message(STATUS "${checking_message}")
if((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
	set(checking_message "${checking_message} [cached] - done")
else((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
	kbuild_try_compile(is_ring_buffer_consume_has_4_args # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_consume_4_args" # bindir
		"ring_buffer_consume_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_CONSUME_HAS_4_ARGS"
		OUTPUT_VARIABLE out_4)

	if(is_ring_buffer_consume_has_4_args)
		set(checking_message "${checking_message} - done")
		set(RING_BUFFER_CONSUME_HAS_4_ARGS "TRUE" CACHE INTERNAL
			"Whether ring_buffer_consume accept 4 arguments")
	else(is_ring_buffer_consume_has_4_args)
		kbuild_try_compile(is_ring_buffer_consume_has_3_args # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_consume_3_args" # bindir
			"ring_buffer_consume_signature.c" # srcfile
			KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_CONSUME_HAS_3_ARGS"
			OUTPUT_VARIABLE out_3)
		if(is_ring_buffer_consume_has_3_args)
			set(checking_message "${checking_message} - done")
			set(RING_BUFFER_CONSUME_HAS_3_ARGS "TRUE" CACHE INTERNAL
			"Whether ring_buffer_consume() accepts 3 arguments")
		else(is_ring_buffer_consume_has_3_args)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unknown signature of ring_buffer_consume().")
			message("Output of test compilation for 3 arguments:")
			message("${out_3}")
			message("Output of test compilation for 4 arguments:")
			message("${out_4}")
			message(FATAL_ERROR "")
		endif(is_ring_buffer_consume_has_3_args)
	endif(is_ring_buffer_consume_has_4_args)
endif((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
message(STATUS "${checking_message}")

# ring_buffer_size
set(checking_message "Checking the signature of ring_buffer_size()")
message(STATUS "${checking_message}")
if((DEFINED RING_BUFFER_SIZE_BUF) OR (DEFINED RING_BUFFER_SIZE_BUF_CPU))
	set(checking_message "${checking_message} [cached] - done")
else((DEFINED RING_BUFFER_SIZE_BUF) OR (DEFINED RING_BUFFER_SIZE_BUF_CPU))
	kbuild_try_compile(is_ring_buffer_size_buf # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_size_buf" # bindir
		"ring_buffer_size_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_SIZE_BUF"
		OUTPUT_VARIABLE out_buf)

	if(is_ring_buffer_size_buf)
		set(checking_message "${checking_message} - done")
		set(RING_BUFFER_SIZE_BUF "TRUE" CACHE INTERNAL
			"Whether ring_buffer_size() accepts only 'buf' argument")
	else(is_ring_buffer_size_buf)
		kbuild_try_compile(is_ring_buffer_size_buf_cpu # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_size_buf_cpu" # bindir
			"ring_buffer_size_signature.c" # srcfile
			KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_SIZE_BUF_CPU"
			OUTPUT_VARIABLE out_buf_cpu)
		if(is_ring_buffer_size_buf_cpu)
			set(checking_message "${checking_message} - done")
			set(RING_BUFFER_SIZE_BUF_CPU "TRUE" CACHE INTERNAL
			"Whether ring_buffer_size() accepts 'buf' and 'cpu' arguments")
		else(is_ring_buffer_size_buf_cpu)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unknown signature of ring_buffer_size().")
			message("Output of test compilation for ring_buffer_size(buf, cpu):")
			message("${out_buf_cpu}")
			message("Output of test compilation for ring_buffer_size(buf)")
			message("${out_buf}")
			message(FATAL_ERROR 
				"Failed to determine the signature of ring_buffer_size()")
		endif(is_ring_buffer_size_buf_cpu)
	endif(is_ring_buffer_size_buf)
endif((DEFINED RING_BUFFER_SIZE_BUF) OR (DEFINED RING_BUFFER_SIZE_BUF_CPU))
message(STATUS "${checking_message}")

# ring_buffer_resize
set(checking_message "Checking the signature of ring_buffer_resize()")
message(STATUS "${checking_message}")
if((DEFINED RING_BUFFER_RESIZE_BUF_SIZE) OR (DEFINED RING_BUFFER_RESIZE_BUF_SIZE_CPU))
	set(checking_message "${checking_message} [cached] - done")
else((DEFINED RING_BUFFER_RESIZE_BUF_SIZE) OR (DEFINED RING_BUFFER_RESIZE_BUF_SIZE_CPU))
	kbuild_try_compile(is_ring_buffer_resize_buf_size # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_resize_buf_size" # bindir
		"ring_buffer_resize_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_RESIZE_BUF_SIZE"
		OUTPUT_VARIABLE out_buf_size)

	if(is_ring_buffer_resize_buf_size)
		set(checking_message "${checking_message} - done")
		set(RING_BUFFER_RESIZE_BUF_SIZE "TRUE" CACHE INTERNAL
			"Whether ring_buffer_resize() accepts 'buf' and 'size' arguments")
	else(is_ring_buffer_resize_buf_size)
		kbuild_try_compile(is_ring_buffer_resize_buf_size_cpu # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_resize_buf_size_cpu" # bindir
			"ring_buffer_resize_signature.c" # srcfile
			KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_RESIZE_BUF_SIZE_CPU"
			OUTPUT_VARIABLE out_buf_size_cpu)
		if(is_ring_buffer_resize_buf_size_cpu)
			set(checking_message "${checking_message} - done")
			set(RING_BUFFER_RESIZE_BUF_SIZE_CPU "TRUE" CACHE INTERNAL
			"Whether ring_buffer_resize() accepts 'buf', 'size' and 'cpu' arguments")
		else(is_ring_buffer_resize_buf_size_cpu)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unknown signature of ring_buffer_resize().")
			message("Output of test compilation for ring_buffer_resize(buf, size, cpu):")
			message("${out_buf_cpu}")
			message("Output of test compilation for ring_buffer_resize(buf, size)")
			message("${out_buf}")
			message(FATAL_ERROR 
				"Failed to determine the signature of ring_buffer_resize()")
		endif(is_ring_buffer_resize_buf_size_cpu)
	endif(is_ring_buffer_resize_buf_size)
endif((DEFINED RING_BUFFER_RESIZE_BUF_SIZE) OR (DEFINED RING_BUFFER_RESIZE_BUF_SIZE_CPU))
message(STATUS "${checking_message}")
# TODO

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${kmodule_name} 
	"kedr_trace_module.c" "trace_buffer.c" "trace_buffer.h" "trace_file.c" "trace_file.h"
)

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})

# The tests
kedr_test_add_subdirectory(tests)
