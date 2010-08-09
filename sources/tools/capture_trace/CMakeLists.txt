#For testing
add_custom_target("kedr_test_capture_trace"
                    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kedr_capture_trace)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_capture_trace" "${CMAKE_CURRENT_SOURCE_DIR}/kedr_capture_trace")

kedr_test_add_target(kedr_test_capture_trace)

add_custom_target("kedr_test_capture_trace_until_done"
                    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kedr_capture_trace_until_done.sh)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_capture_trace_until_done.sh" "${CMAKE_CURRENT_SOURCE_DIR}/kedr_capture_trace_until_done.sh")

kedr_test_add_target(kedr_test_capture_trace_until_done)


install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/kedr_capture_trace" "${CMAKE_CURRENT_SOURCE_DIR}/kedr_capture_trace_until_done.sh"
			DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
            PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)
