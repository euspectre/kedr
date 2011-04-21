set(kedr_stack_trace_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/..")
if (STACK_TRACE_RELIABLE)
	add_subdirectory(reliability)
endif()

