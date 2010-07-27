kedr_load_install_prefixes()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/kedr_capture_trace.in"
    ${CMAKE_CURRENT_BINARY_DIR}/kedr_capture_trace
    @ONLY)

# For testing
kedr_load_test_prefixes()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/kedr_capture_trace.in"
    ${CMAKE_CURRENT_BINARY_DIR}/kedr_test_capture_trace
    @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr_capture_trace" "${CMAKE_CURRENT_SOURCE_DIR}/kedr_capture_trace_until_done.sh"
			DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
            PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)

add_subdirectory(tests)