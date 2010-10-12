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
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif ()
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    kbuild_add_cflags(
        "-DKEDR_DEBUG"
    )
endif()

#######################################################################
# kedr_gen is built here explicitly and installed to a temporary location.
# This is only necessary to build KEDR itself.
# This should be done before the first add_subdirectory() command.

# The temporary install directory must be persistent and in must not be 
# KEDR_INSTALL_PREFIX_TEMP because it has nothing to do with the installation 
# of KEDR: it is used when KEDR is being built only.

message (STATUS "Creating \"kedr_gen\"")
set (KEDR_GEN_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/kedr_gen.install")
set (KEDR_GEN_TEMP_BUILD "${CMAKE_CURRENT_BINARY_DIR}/kedr_gen.build")

# These temporary directories will be removed during make clean
set_property (DIRECTORY APPEND PROPERTY 
    ADDITIONAL_MAKE_CLEAN_FILES kedr_gen.build
)
#set_property (DIRECTORY APPEND PROPERTY 
#    ADDITIONAL_MAKE_CLEAN_FILES kedr_gen.install
#)

# Create the temporary build and install directories first
execute_process (
    COMMAND rm -rf "${KEDR_GEN_TEMP_BUILD}" 
    COMMAND mkdir -p "${KEDR_GEN_TEMP_BUILD}" 
    RESULT_VARIABLE kedr_gen_result
)
if (NOT kedr_gen_result EQUAL 0)
    message (FATAL_ERROR 
"Failed to create directory ${KEDR_GEN_TEMP_BUILD}"
    )
endif ()

execute_process (
    COMMAND rm -rf "${KEDR_GEN_INSTALL_PREFIX}" 
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
    message ("CMake exit code:\n${kedr_gen_result}\n")
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

# This file is used for create configuration file for call monitor payloads
set(kedr_cm_conf_file_data "${CMAKE_BINARY_DIR}/tools/control/kedr_cm.conf.data")

# tools should be before payloads
add_subdirectory(tools)
add_subdirectory(payloads_callm)

add_subdirectory(payloads_fsim)

add_subdirectory(examples)
# Documentation
add_subdirectory(doc)

# "Global" tests
add_subdirectory(tests)
#######################################################################
