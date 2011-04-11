add_executable(kedr_capture_trace kedr_capture_trace.c)

install(TARGETS kedr_capture_trace
    DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}")

kedr_test_add_subdirectory(tests)
