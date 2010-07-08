#For testing
add_custom_target("kedr_test_capture_trace"
                    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/capture_trace)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/capture_trace" "${CMAKE_CURRENT_SOURCE_DIR}/capture_trace")

add_dependencies(build_tests kedr_test_capture_trace)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/capture_trace"
			DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
            PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)