include(kedr_testing)
kedr_test_init()

# Tests configuration may take considerable amount of time.
# So print status messages about tests configuration.
message(STATUS "Configuring tests")

# This directory(and its subdirectories) should be used in tests for
# temporary files and directories, especially for mount debugfs.
#
# See rdc57abf8b657 for details about why this is needed.
set(KEDR_TEST_PREFIX_TEMP_SESSION "/var/tmp/kedr/test")

if(USER_PART)
    # Do not forget to delete this dir on 'make uninstall'.
    add_uninstall_dir(${KEDR_TEST_PREFIX_TEMP_SESSION})
endif(USER_PART)

# Install auxiliary test scripts for common using.
if(USER_PART)
    add_subdirectory(scripts)
endif(USER_PART)
itesting_path(TEST_SCRIPTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/scripts")

if(KERNEL_PART)
    # Install auxiliary test modules for common using.
    add_subdirectory(modules)
endif(KERNEL_PART)
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
