set(kmodule_name "kedr_trace")
set(kmodule "${kmodule_name}.ko")

# ring_buffer_consume
check_begin("Checking the signature of ring_buffer_consume()")

if((DEFINED RING_BUFFER_CONSUME_HAS_4_ARGS) OR (DEFINED RING_BUFFER_CONSUME_HAS_3_ARGS))
	set(found "TRUE")
else()
	set(found)
endif()

if(NOT found)
	check_try()
	kbuild_try_compile(is_ring_buffer_consume_has_4_args # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_consume_4_args" # bindir
		"ring_buffer_consume_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_CONSUME_HAS_4_ARGS"
	)

	if(is_ring_buffer_consume_has_4_args)
		set(RING_BUFFER_CONSUME_HAS_4_ARGS "TRUE" CACHE INTERNAL
			"Whether ring_buffer_consume accept 4 arguments"
		)
		set(found "TRUE")
	endif()
endif(NOT found)

if(NOT found)
	check_try()
	kbuild_try_compile(is_ring_buffer_consume_has_3_args # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_consume_3_args" # bindir
		"ring_buffer_consume_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_CONSUME_HAS_3_ARGS"
	)
	if(is_ring_buffer_consume_has_3_args)
		set(RING_BUFFER_CONSUME_HAS_3_ARGS "TRUE" CACHE INTERNAL
			"Whether ring_buffer_consume() accepts 3 arguments"
		)
		set(found "TRUE")
	endif()
endif(NOT found)

if(RING_BUFFER_CONSUME_HAS_4_ARGS)
	check_end("accept 'lost_events' argument")
elseif(RING_BUFFER_CONSUME_HAS_3_ARGS)
	check_end("does not accept 'lost_events' argument")
else(RING_BUFFER_CONSUME_HAS_4_ARGS)
	check_end("unknown")
	message(FATAL_ERROR "Unknown signature of ring_buffer_consume().")
endif(RING_BUFFER_CONSUME_HAS_4_ARGS)

# ring_buffer_size
check_begin("Checking the signature of ring_buffer_size()")
if((DEFINED RING_BUFFER_SIZE_BUF) OR (DEFINED RING_BUFFER_SIZE_BUF_CPU))
	set(found "TRUE")
else()
	set(found)
endif()

if(NOT found)
	check_try()
	kbuild_try_compile(is_ring_buffer_size_buf_cpu # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_size_buf_cpu" # bindir
		"ring_buffer_size_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_SIZE_BUF_CPU"
	)
	if(is_ring_buffer_size_buf_cpu)
		set(RING_BUFFER_SIZE_BUF_CPU "TRUE" CACHE INTERNAL
			"Whether ring_buffer_size() accepts 'buf' and 'cpu' arguments"
		)
		set(found "TRUE")
	endif()
endif(NOT found)

if(NOT found)
	check_try()
	kbuild_try_compile(is_ring_buffer_size_buf # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_size_buf" # bindir
		"ring_buffer_size_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_SIZE_BUF"
	)

	if(is_ring_buffer_size_buf)
		set(RING_BUFFER_SIZE_BUF "TRUE" CACHE INTERNAL
			"Whether ring_buffer_size() accepts only 'buf' argument"
		)
		set(found "TRUE")
	endif()
endif(NOT found)

if(RING_BUFFER_SIZE_BUF_CPU)
	check_end("accept 'cpu' argument")
elseif(RING_BUFFER_SIZE_BUF)
	check_end("does not accept 'cpu' argument")
else(RING_BUFFER_SIZE_BUF_CPU)
	check_end("unknown")
	message(FATAL_ERROR "Unknown signature of ring_buffer_size().")
endif(RING_BUFFER_SIZE_BUF_CPU)

# ring_buffer_resize
check_begin("Checking the signature of ring_buffer_resize()")

if((DEFINED RING_BUFFER_RESIZE_BUF_SIZE) OR (DEFINED RING_BUFFER_RESIZE_BUF_SIZE_CPU))
	set(found "TRUE")
else()
	set(found)
endif()

if(NOT found)
	check_try()
	kbuild_try_compile(is_ring_buffer_resize_buf_size_cpu # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_resize_buf_size_cpu" # bindir
		"ring_buffer_resize_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_RESIZE_BUF_SIZE_CPU"
		OUTPUT_VARIABLE out_buf_size_cpu)
	if(is_ring_buffer_resize_buf_size_cpu)
		set(RING_BUFFER_RESIZE_BUF_SIZE_CPU "TRUE" CACHE INTERNAL
			"Whether ring_buffer_resize() accepts 'buf', 'size' and 'cpu' arguments"
		)
		set(found "TRUE")
	endif()
endif(NOT found)

if(NOT found)
	check_try()
	kbuild_try_compile(is_ring_buffer_resize_buf_size # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/ring_buffer_resize_buf_size" # bindir
		"ring_buffer_resize_signature.c" # srcfile
		KBUILD_COMPILE_DEFINITIONS "-DRING_BUFFER_RESIZE_BUF_SIZE"
	)

	if(is_ring_buffer_resize_buf_size)
		set(RING_BUFFER_RESIZE_BUF_SIZE "TRUE" CACHE INTERNAL
			"Whether ring_buffer_resize() accepts 'buf' and 'size' arguments"
		)
		set(found "TRUE")
	endif()
endif(NOT found)

if(RING_BUFFER_RESIZE_BUF_SIZE_CPU)
	check_end("accept 'cpu_id' argument")
elseif(RING_BUFFER_RESIZE_BUF_SIZE)
	check_end("does not accept 'cpu_id' argument")
else(RING_BUFFER_RESIZE_BUF_SIZE_CPU)
	check_end("unknown")
	message(FATAL_ERROR "Unknown signature of ring_buffer_resize().")
endif(RING_BUFFER_RESIZE_BUF_SIZE_CPU)

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${kmodule_name} 
	"kedr_trace_module.c" "trace_buffer.c" "trace_buffer.h" "trace_file.c" "trace_file.h"
)

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})
