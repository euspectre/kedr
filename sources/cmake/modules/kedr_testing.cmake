########################################################################
# Test-related macros
########################################################################

# When we are building KEDR for another system (cross-build), testing is
# disabled. This is because the tests need the build tree.
# In the future, the tests could be prepared that need only the installed 
# components of KEDR. It could be a separate test suite.

# This macro enables testing support and performs other initialization tasks.
# It should be used in the top-level CMakeLists.txt file before 
# add_subdirectory () calls.
macro (kedr_test_init)
    if (NOT CMAKE_CROSSCOMPILING)
        enable_testing ()
        add_custom_target (check 
            COMMAND ${CMAKE_CTEST_COMMAND}
        )
        add_custom_target (build_tests)
        add_dependencies (check build_tests)
    endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_init)

# Use this macro to specify an additional target to be built before the tests
# are executed.
macro (kedr_test_add_target target_name)
    if (NOT CMAKE_CROSSCOMPILING)
        set_target_properties (${target_name}
            PROPERTIES EXCLUDE_FROM_ALL true
        )
        add_dependencies (build_tests ${target_name})
    endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_add_target target_name)

# This function adds a test script (a Bash script, actually) to the set of
# tests for the package. The script may reside in current source or binary 
# directory (the source directory is searched first).
function (kedr_test_add_script test_name script_file)
    if (NOT CMAKE_CROSSCOMPILING)
        to_abs_path (TEST_SCRIPT_FILE ${script_file})
            
        add_test (${test_name}
            /bin/bash ${TEST_SCRIPT_FILE} ${ARGN}
        )
    endif (NOT CMAKE_CROSSCOMPILING)
endfunction (kedr_test_add_script)

function (kedr_test_add test_name app_file)
    if (NOT CMAKE_CROSSCOMPILING)
        to_abs_path (TEST_APP_FILE ${app_file})
            
        add_test (${test_name} ${TEST_APP_FILE} ${ARGN})
    endif (NOT CMAKE_CROSSCOMPILING)
endfunction (kedr_test_add)

# Use this macro instead of add_subdirectory() for the subtrees related to 
# testing of the package.

# We could use other kedr_*test* macros to disable the tests when 
# cross-building, but the rules of Kbuild system (concerning .symvers,
# etc.) still need to be disabled explicitly. So it is more reliable to 
# just turn off each add_subdirectory(tests) in this case.
macro (kedr_test_add_subdirectory subdir)
    if (NOT CMAKE_CROSSCOMPILING)
        add_subdirectory(${subdir})
    endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_add_subdirectory subdir)

########################################################################
# Swithing variables values for install variant and for tests.
#
# Delivarables should use KEDR_PREFX_* variables for being configurable
# both for install and for testing.
#
# When configured for install, these variables take values from
# KEDR_INSTALL_PREFIX_* variables.
# When configured for testing, value of KEDR_TEST_PREFIX_* is used.
# 
# List of all variables(suffixes only), which value will be switch
# kedr_load_{install|test}_prefixes().
set (KEDR_ALL_PATH_SUFFIXES EXEC READONLY GLOBAL_CONF LIB INCLUDE 
	TEMP_SESSION TEMP STATE CACHE VAR DOC 
	KMODULE KSYMVERS KINCLUDE EXAMPLES TEMPLATES)

########################################################################
# Path prefixes for tests
# Normally, them same as for install, but with additional directory prefix.
set(KEDR_TEST_COMMON_PREFIX "/var/tmp/${KEDR_PACKAGE_NAME}/test")

foreach(var_suffix ${KEDR_ALL_PATH_SUFFIXES})
	set(KEDR_TEST_PREFIX_${var_suffix} "${KEDR_TEST_COMMON_PREFIX}${KEDR_INSTALL_PREFIX_${var_suffix}}")
endforeach(var_suffix ${KEDR_ALL_PATH_SUFFIXES})
# But some path prefixes for tests are special:
# Root of include tree in building package
set(KEDR_TEST_PREFIX_INCLUDE "${CMAKE_BINARY_DIR}/include")

set(KEDR_TEST_PREFIX_TEMPLATES "${CMAKE_SOURCE_DIR}/templates")


# kedr_load_install_prefixes()
# Set common prefixes variables equal to ones in install mode (should be 
# called before configure files, which use prefixes)
macro(kedr_load_install_prefixes)
	foreach(var_suffix ${KEDR_ALL_PATH_SUFFIXES})
		set(KEDR_PREFIX_${var_suffix} ${KEDR_INSTALL_PREFIX_${var_suffix}})
	endforeach(var_suffix ${KEDR_ALL_PATH_SUFFIXES})
endmacro(kedr_load_install_prefixes)

# kedr_load_test_prefixes()
# Set common prefixes variables equal to ones in test mode(should be called 
# before configure files, which use prefixes)
macro(kedr_load_test_prefixes)
	foreach(var_suffix ${KEDR_ALL_PATH_SUFFIXES})
		set(KEDR_PREFIX_${var_suffix} ${KEDR_TEST_PREFIX_${var_suffix}})
	endforeach(var_suffix ${KEDR_ALL_PATH_SUFFIXES})
endmacro(kedr_load_test_prefixes)
