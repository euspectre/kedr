add_subdirectory(target)
add_subdirectory(payload)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

kedr_test_add_script("capture_trace.01" "${CMAKE_CURRENT_BINARY_DIR}/test.sh" kedr_capture_trace)
kedr_test_add_script("capture_trace_until_done.01" "${CMAKE_CURRENT_BINARY_DIR}/test.sh" kedr_capture_trace_until_done)