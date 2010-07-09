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

#######################################################################
# See conventions
set(KEDR_INSTALL_PREFIX_LIB "${CMAKE_INSTALL_PREFIX}/lib")
set(KEDR_INSTALL_PREFIX_INCLUDE "${CMAKE_INSTALL_PREFIX}/include/kedr")
#set(KEDR_INSTALL_PREFIX_KINCLUDE "${CMAKE_INSTALL_PREFIX}/include/kedr")
set(KEDR_INSTALL_PREFIX_KMODULE "${CMAKE_INSTALL_PREFIX}/lib/modules/${KBUILD_VERSION_STRING}/misc")
set(KEDR_INSTALL_PREFIX_KSYMVERS "${CMAKE_INSTALL_PREFIX}/lib/modules/${KBUILD_VERSION_STRING}/symvers")
#
string(REGEX MATCH "^/opt|^/usr|^/$" IS_GLOBAL_INSTALL ${CMAKE_INSTALL_PREFIX})
if(IS_GLOBAL_INSTALL)
    message("Global installation")
    set(KEDR_INSTALL_PREFIX_VAR "/var/opt/kedr")
else(IS_GLOBAL_INSTALL)
    message("Local installation")
    set(KEDR_INSTALL_PREFIX_VAR "${CMAKE_INSTALL_PREFIX}/var")
endif(IS_GLOBAL_INSTALL)
set(KEDR_INSTALL_PREFIX_EXEC "${CMAKE_INSTALL_PREFIX}/bin")

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
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

#######################################################################
#${CMAKE_SOURCE_DIR} - root of the include tree (common and internal).
include_directories(${CMAKE_SOURCE_DIR})
kbuild_include_directories(${CMAKE_SOURCE_DIR})

#######################################################################
# Make "Release" the default build type
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    kbuild_add_cflags(
        "-DKEDR_DEBUG"
    )
endif()

#######################################################################
add_subdirectory(base)

add_subdirectory(controller)

add_subdirectory(module_weak_ref)
add_subdirectory(syscall_connector)
add_subdirectory(fault_simulation)

add_subdirectory(kedr)

add_subdirectory(mist_gen)

#Configuration-files for collect payloads
set(kedr_cm_conf_filename "kedr_cm.conf")
set(kedr_cm_conf_file "${CMAKE_BINARY_DIR}/tools/control/${kedr_cm_conf_filename}")
set(kedr_test_cm_conf_filename "kedr_cm_test.conf")
set(kedr_test_cm_conf_file "${CMAKE_BINARY_DIR}/tools/control/${kedr_test_cm_conf_filename}")
# Directory for local mount debugfs for enable payloads tracepoints and for use by capturing trace script)
# This directory is used in configuration file for control KEDR service.
set(kedr_debugfs_dir "/tmp/kedr/debugfs")
# Directory for local mount debugfs for kedr_controller.
# This directory will be used only in service script(internally), and should DIFFER from previous one,
# because script itself do not now, whether this directory already used for mount point in conf file, or not.
set(kedr_controller_debugfs_dir "/tmp/kedr/controller_debugfs")

# tools should be before payloads
add_subdirectory(tools)
add_subdirectory(payloads_callm)

add_subdirectory(examples)
# Documentation
add_subdirectory(doc)

# "Global" tests
add_subdirectory(tests)
#######################################################################
