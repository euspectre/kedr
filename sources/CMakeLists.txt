cmake_minimum_required(VERSION 2.6)

enable_language(C)
enable_language(CXX)

#######################################################################
# Prohibit a common type of an in-source build.
# Note that building in a subdirectory in the source tree is still allowed 
# as it can be convenient.
string (COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" in_source)
if (in_source)
    message (FATAL_ERROR 
"It is not allowed to build the project in its top source directory."
)
endif () 

#######################################################################
# Names and versions
set(KEDR_PACKAGE_NAME "kedr")
set(KEDR_VERSION_MAJOR 0)
set(KEDR_VERSION_MINOR 1)
set(KEDR_VERSION_MICRO 0)

#######################################################################
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
find_package(Kbuild)

include(cmake_useful)
include(kbuild_system)
include(kmodule)

include(path_prefixes)
#######################################################################
#######################################################################
# Initialize test-related stuff
kedr_test_init ()

#######################################################################
#kedr_install_library(library_name)
function(kedr_install_library library_name)
	install(TARGETS ${library_name} LIBRARY
			DESTINATION ${KEDR_INSTALL_PREFIX_LIB})
endfunction(kedr_install_library library_name)
#kedr_install_headers(install_subdir header_file [..])
function(kedr_install_headers install_subdir)
	install(FILES ${header_file} ${ARGN}
			DESTINATION ${KEDR_INSTALL_PREFIX_INCLUDE}/${install_subdir})
endfunction(kedr_install_headers install_subdir)
#kedr_install_kmodule(kmodule_name)
function(kedr_install_kmodule kmodule_name)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${kmodule_name}.ko"
			DESTINATION "${KEDR_INSTALL_PREFIX_KMODULE}")
endfunction(kedr_install_kmodule kmodule_name)
#kedr_install_symvers(kmodule_name)
function(kedr_install_symvers kmodule_name)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Module.symvers"
			DESTINATION "${KEDR_INSTALL_PREFIX_KSYMVERS}"
			RENAME "${kmodule_name}.symvers")
endfunction(kedr_install_symvers kmodule_name)

#######################################################################
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

add_custom_target (uninstall_files
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

add_custom_target (uninstall)

# During uninstall process, the files should be removed first, then 
# the directories.
# 'uninstall_dirs' target is defined in cmake/modules/path_prefixes.cmake.
add_dependencies (uninstall_dirs uninstall_files)
add_dependencies (uninstall uninstall_dirs)

#######################################################################
#${CMAKE_SOURCE_DIR}/include - root of the include tree (see convensions).
include_directories("${CMAKE_SOURCE_DIR}/include")
kbuild_include_directories("${CMAKE_SOURCE_DIR}/include")

#######################################################################
# Make "Release" the default build type
set (CMAKE_BUILD_TYPE "Release" CACHE STRING "Types of build, options are: None Debug Release RelWithDebInfo MinSizeRel.")
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    kbuild_add_cflags(
        "-DKEDR_DEBUG"
    )
endif()

#######################################################################
# Configuration of payloads set
option(KEDR_STANDARD_CALLM_PAYLOADS "Whether need to build standard set of call monitor payloads" ON)
option(KEDR_STANDARD_FSIM_PAYLOADS "Whether need to build standard set of fault simulation payloads" ON)
#######################################################################
# kedr_gen is built here explicitly and installed to a temporary location.
# This is only necessary to build KEDR itself.
# This should be done before the first add_subdirectory() command.

# The temporary install directory must be persistent and it must not be 
# KEDR_INSTALL_PREFIX_TEMP because it has nothing to do with the installation 
# of KEDR: it is used when KEDR is being built only.

message (STATUS "Creating \"kedr_gen\"")
set (KEDR_GEN_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/kedr_gen.install")
set (KEDR_GEN_TEMP_BUILD "${CMAKE_CURRENT_BINARY_DIR}/kedr_gen.build")

# The temporary build directory should be removed during 'make clean'
# Note that the temporary install directory containing 'kedr_gen' must 
# survive as kedr_gen will be necessary if the user wants to rebuild KEDR 
# after 'make clean' without reconfiguring.
set_property (DIRECTORY APPEND PROPERTY 
    ADDITIONAL_MAKE_CLEAN_FILES kedr_gen.build
)

# Remove the old temporary build and install directories first and 
# (re)create them.

# [NB] Note that if several commands are specified in execute_process() 
# call, it is equivalent to executing a pipeline in the shell:
#   command1 | command2 | ...
# Generally, there are no restrictions on the order in which the commands 
# will be executed and on whether some of the commands will execute 
# concurrently. Therefore, here is the rule of thumb:
# [!!!] Specify 2 or more commands in a single execute_process() statement
# if and only if the pipeline of the commands is what you want or if it is 
# not important whether they will actually run simultaneously. 
# Otherwise, you should use separate execute_process() statements.
#
# For example, using execute_process() like this is definitely looking 
# for trouble:
#   execute_process (
#       COMMAND rm -rf "some_dir" 
#       COMMAND mkdir -p "some_dir" 
#   )
# The order in which these commands will do their work is unspecified. 
# It may happen that 'mkdir' will finish before 'rm' will begin removing
# "some_dir". This is probably not what you expect here.

#Do not destroy previously installed generator
#execute_process (
#    COMMAND rm -rf "${KEDR_GEN_TEMP_BUILD}" 
#    COMMAND rm -rf "${KEDR_GEN_INSTALL_PREFIX}" 
#)

# [NB] After the command is executed, the variable specified 
# in RESULT_VARIABLE may contain either numeric return code or a string 
# describing the error.
execute_process (
    COMMAND mkdir -p "${KEDR_GEN_TEMP_BUILD}" 
    RESULT_VARIABLE kedr_gen_result
)
if (NOT kedr_gen_result EQUAL 0)
    message (FATAL_ERROR 
"Failed to create directory ${KEDR_GEN_TEMP_BUILD}"
    )
endif ()

execute_process (
    COMMAND mkdir -p "${KEDR_GEN_INSTALL_PREFIX}" 
    RESULT_VARIABLE kedr_gen_result
)
if (NOT kedr_gen_result EQUAL 0)
    message (FATAL_ERROR 
"Failed to create directory ${KEDR_GEN_TEMP_BUILD}"
    )
endif ()

# Configure kedr_gen
execute_process (
    COMMAND ${CMAKE_COMMAND} 
        -DCMAKE_INSTALL_PREFIX=${KEDR_GEN_INSTALL_PREFIX}
        -DCMAKE_BUILD_TYPE=Release
        -DKEDR_GEN_INSTALL_PREFIX=${KEDR_GEN_INSTALL_PREFIX}
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/kedr_gen/src"
    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
    RESULT_VARIABLE kedr_gen_result
    OUTPUT_VARIABLE kedr_gen_output
    ERROR_VARIABLE  kedr_gen_output
)
if (NOT kedr_gen_result EQUAL 0)
    message ("Failed to configure \"kedr_gen\".")
    message ("CMake output:\n${kedr_gen_output}\n")
    message ("CMake result:\n${kedr_gen_result}\n")
    message (FATAL_ERROR "Unable to build \"kedr_gen\", aborting.")
endif ()

# Build kedr_gen
execute_process (
    COMMAND make
    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
    RESULT_VARIABLE kedr_gen_result
    OUTPUT_VARIABLE kedr_gen_output
    ERROR_VARIABLE  kedr_gen_output
)
if (NOT kedr_gen_result EQUAL 0)
    message ("Failed to build \"kedr_gen\".")
    message ("Make output:\n${kedr_gen_output}\n")
    message ("Make result:\n${kedr_gen_result}\n")
    message (FATAL_ERROR "Unable to build \"kedr_gen\", aborting.")
endif ()

# Install kedr_gen
execute_process (
    COMMAND make install
    WORKING_DIRECTORY ${KEDR_GEN_TEMP_BUILD}
    RESULT_VARIABLE kedr_gen_result
    OUTPUT_VARIABLE kedr_gen_output
    ERROR_VARIABLE  kedr_gen_output
)
if (NOT kedr_gen_result EQUAL 0)
    message ("Failed to install \"kedr_gen\" to ${KEDR_GEN_INSTALL_PREFIX}.")
    message ("Make output:\n${kedr_gen_output}\n")
    message ("Make result:\n${kedr_gen_result}\n")
    message (FATAL_ERROR "Unable to install \"kedr_gen\", aborting.")
endif ()

# ${KEDR_GEN_TOOL} is the path to "kedr_gen". The path can be used to call
# "kedr_gen" tool during the build of payload modules, etc.
set (KEDR_GEN_TOOL ${KEDR_GEN_INSTALL_PREFIX}/kedr_gen)
message (STATUS "Creating \"kedr_gen\" - done")

#######################################################################
add_subdirectory(base)

# This file will be used for tests, whether controller really intercept
# particular functions calls
set(function_triggers_file "${CMAKE_BINARY_DIR}/controller/tests/call_interception/function_triggers.data")
add_subdirectory(controller)

# For tests only
add_subdirectory(wobject)
add_subdirectory(control_file)

add_subdirectory(fault_simulation)

add_subdirectory(include)

# This file is used for create configuration file for call monitor payloads or empty one
set(kedr_conf_file_data_default "${CMAKE_BINARY_DIR}/tools/control/kedr.conf.data")
set(kedr_test_conf_file_data_default "${CMAKE_BINARY_DIR}/tools/control/kedr_test.conf.data")

if(KEDR_STANDARD_FSIM_PAYLOADS)
# This file is used for create configuration file for fault simulation payloads
set(kedr_conf_file_data_fsim "${CMAKE_BINARY_DIR}/tools/control/kedr_fsim.conf.data")
endif(KEDR_STANDARD_FSIM_PAYLOADS)

# tools should be before payloads
add_subdirectory(tools)

if(KEDR_STANDARD_CALLM_PAYLOADS)
add_subdirectory(payloads_callm)
endif(KEDR_STANDARD_CALLM_PAYLOADS)

if(KEDR_STANDARD_FSIM_PAYLOADS)
add_subdirectory(payloads_fsim)
add_subdirectory(fault_indicators)
endif(KEDR_STANDARD_FSIM_PAYLOADS)

add_subdirectory(examples)
# Documentation
add_subdirectory(doc)

# "Global" tests
add_subdirectory(tests)
#######################################################################
