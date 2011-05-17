set(program_name "kedr_capture_trace")
add_executable("${program_name}" kedr_capture_trace.c)

install(TARGETS "${program_name}"
    DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}")

kedr_test_add_subdirectory(tests)
