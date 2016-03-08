kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

set(TRACE_TEST_GENERATOR_MODULE_NAME "trace_test_generator")
itesting_path(TRACE_TEST_GENERATOR_MODULE
    "${CMAKE_CURRENT_BINARY_DIR}/modules/trace_generator/${TRACE_TEST_GENERATOR_MODULE_NAME}.ko")

set(TRACE_TEST_TARGET_MODULE_NAME "trace_test_target")
itesting_path(TRACE_TEST_TARGET_MODULE
    "${CMAKE_CURRENT_BINARY_DIR}/modules/trace_target/${TRACE_TEST_TARGET_MODULE_NAME}.ko")

add_subdirectory(modules)

# Common mount point for debugfs.
set(DEBUGFS_MOUNT_POINT "${KEDR_TEST_PREFIX_TEMP_SESSION}/kedr_trace_debugfs")

configure_file("kedr_trace_test.conf.in" "kedr_trace_test.conf"
    @ONLY
)
itesting_path(KEDR_TRACE_TEST_CONF_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/kedr_trace_test.conf"
)

configure_file("test_common.sh.in" "test_common.sh"
    @ONLY
)
itesting_path(KEDR_TRACE_TEST_COMMON_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/test_common.sh"
)


kedr_test_install(FILES "kedr_trace_test.conf" "test_common.sh")

kedr_test_install(PROGRAMS "verify_trace_format.awk")

configure_file("test_block.sh.in" "test_block.sh" @ONLY)
kedr_test_add_script("kedr_trace.blocking_read.01" "test_block.sh")

configure_file("test_session.sh.in" "test_session.sh" @ONLY)
kedr_test_add_script("kedr_trace.session.01" "test_session.sh")

configure_file("test_block_session.sh.in" "test_block_session.sh" @ONLY)
kedr_test_add_script("kedr_trace.block_session.01" "test_block_session.sh")


add_subdirectory(simple_ordering)
add_subdirectory(cross_cpu_ordering)