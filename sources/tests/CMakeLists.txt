# Base directory where tests will be installed.
#
# Test tree is per-kernel and will be installed under
# `uname -r` subdirectory.
set(KEDR_INSTALL_TEST_DIR "${KEDR_INSTALL_PREFIX_VAR}/tests")

if(USER_PART)
    # Create common run_tests.sh script, which will be located in
    # kernel-independent directory.

    # NB: "run_tests.sh" in current binary dir will be created by
    # ictest_enable_testing() called from kedr_test_init().
    #
    # But that file will be installed under `uname -r` subdirectory.
    #
    # So use temporary name for common run_tests.sh in build directory,
    # but rename it to it final name on install.
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/run_tests_common.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/run_tests_common.sh")
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/run_tests_common.sh"
        DESTINATION ${KEDR_INSTALL_TEST_DIR}
        RENAME "run_tests.sh"
    )
endif(USER_PART)

if(NOT KERNEL_PART)
# Test tree itself is fully per-kernel.
    return()
endif(NOT KERNEL_PART)

include(kedr_testing)
kedr_test_init(${KEDR_INSTALL_TEST_DIR}/${Kbuild_VERSION_STRING})

# Tests configuration may take considerable amount of time.
# So print status messages about tests configuration.
message(STATUS "Configuring tests")

# This directory(and its subdirectories) should be used in tests for
# temporary files and directories, especially for mount debugfs.
#
# See rdc57abf8b657 for details about why this is needed.
set(KEDR_TEST_PREFIX_TEMP_SESSION "/var/tmp/kedr/test/${Kbuild_VERSION_STRING}")

# Do not forget to delete this dir on 'make uninstall'.
add_uninstall_dir(${KEDR_TEST_PREFIX_TEMP_SESSION})

# Install auxiliary test scripts for common using.
add_subdirectory(scripts)
itesting_path(TEST_SCRIPTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/scripts")

# Install auxiliary test modules for common using.
add_subdirectory(modules)
itesting_path(TEST_MODULES_DIR "${CMAKE_CURRENT_BINARY_DIR}/modules")

add_subdirectory(core)
add_subdirectory(control_file)
add_subdirectory(calculator)

add_subdirectory(fault_simulation)

add_subdirectory(leak_check)

if(KEDR_TRACE)
    add_subdirectory(trace)
endif(KEDR_TRACE)

if(KEDR_STANDARD_CALLM_PAYLOADS)
    add_subdirectory(payloads_callm)
endif(KEDR_STANDARD_CALLM_PAYLOADS)

if(KEDR_STANDARD_FSIM_PAYLOADS)
    add_subdirectory(payloads_fsim)
    add_subdirectory(fault_indicators)
endif(KEDR_STANDARD_FSIM_PAYLOADS)

add_subdirectory(tools)
add_subdirectory(util)

if (NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(examples)
endif (NOT CMAKE_CROSSCOMPILING)


add_subdirectory(global)

message(STATUS "Configuring tests - done")
