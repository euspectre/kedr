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
# See conventions about paths of installed files
# Determine type of installation
string(REGEX MATCH "(^/opt|^/usr|^/$)" IS_GLOBAL_INSTALL ${CMAKE_INSTALL_PREFIX})
if(IS_GLOBAL_INSTALL)
	set(KEDR_INSTALL_TYPE "global")
    set(KEDR_INSTALL_PREFIX_VAR "/var/opt/kedr")
	if(CMAKE_MATCH_1 STREQUAL "/opt")
		message("Global installation into /opt")
		set(KEDR_INSTALL_GLOBAL_IS_OPT "opt")
	else(CMAKE_MATCH_1 STREQUAL "/opt")
	    message("Global installation")
	endif(CMAKE_MATCH_1 STREQUAL "/opt")
else(IS_GLOBAL_INSTALL)
    message("Local installation")
	set(KEDR_INSTALL_TYPE "local")
    set(KEDR_INSTALL_PREFIX_VAR "${CMAKE_INSTALL_PREFIX}/var")
endif(IS_GLOBAL_INSTALL)

# Set prefixes
# 1
set(KEDR_INSTALL_PREFIX_EXEC
		"${CMAKE_INSTALL_PREFIX}/bin")
set(KEDR_INSTALL_PREFIX_EXEC_AUX
		"${CMAKE_INSTALL_PREFIX}/lib/${KEDR_PACKAGE_NAME}")
# 2
set(KEDR_INSTALL_PREFIX_READONLY
		"${CMAKE_INSTALL_PREFIX}/share/${KEDR_PACKAGE_NAME}")
set(KEDR_INSTALL_PREFIX_MANPAGE
		"${CMAKE_INSTALL_PREFIX}/share/man")
# 3
if(KEDR_INSTALL_TYPE STREQUAL "global")
	set(KEDR_PREFIX_GLOBAL_CONF
			"/etc/${KEDR_PACKAGE_NAME}")
else(KEDR_INSTALL_TYPE STREQUAL "global")
	set(KEDR_PREFIX_GLOBAL_CONF
			"${CMAKE_INSTALL_PREFIX}/etc/${KEDR_PACKAGE_NAME}")
endif(KEDR_INSTALL_TYPE STREQUAL "global")
# 4
set(KEDR_INSTALL_PREFIX_LIB
		"${CMAKE_INSTALL_PREFIX}/lib")
set(KEDR_INSTALL_PREFIX_LIB_AUX
		"${CMAKE_INSTALL_PREFIX}/lib/${KEDR_PACKAGE_NAME}")
# 5
set(KEDR_INSTALL_PREFIX_INCLUDE
		"${CMAKE_INSTALL_PREFIX}/include/${KEDR_PACKAGE_NAME}")
# 6
set(KEDR_PREFIX_TEMP_SESSION
			"/tmp/${KEDR_PACKAGE_NAME}")
# 7
if(KEDR_INSTALL_TYPE STREQUAL "global")
	set(KEDR_PREFIX_TEMP
				"/var/tmp/${KEDR_PACKAGE_NAME}")
else(KEDR_INSTALL_TYPE STREQUAL "global")
	set(KEDR_PREFIX_TEMP_SESSION
				"${CMAKE_INSTALL_PREFIX}/var/tmp/${KEDR_PACKAGE_NAME}")
endif(KEDR_INSTALL_TYPE STREQUAL "global")
# 8
if(KEDR_INSTALL_TYPE STREQUAL "global")
	if(KEDR_INSTALL_GLOBAL_IS_OPT)
		set(KEDR_PREFIX_STATE
			"/var/opt/${KEDR_PACKAGE_NAME}/lib/${KEDR_PACKAGE_NAME}")
	else(KEDR_INSTALL_GLOBAL_IS_OPT)
		set(KEDR_PREFIX_STATE
			"/var/lib/${KEDR_PACKAGE_NAME}")
	endif(KEDR_INSTALL_GLOBAL_IS_OPT)
else(KEDR_INSTALL_TYPE STREQUAL "global")
	set(KEDR_PREFIX_STATE
		"${CMAKE_INSTALL_PREFIX}/var/lib/${KEDR_PACKAGE_NAME}")
endif(KEDR_INSTALL_TYPE STREQUAL "global")
# 9
if(KEDR_INSTALL_TYPE STREQUAL "global")
	if(KEDR_INSTALL_GLOBAL_IS_OPT)
		set(KEDR_PREFIX_CACHE
			"/var/opt/${KEDR_PACKAGE_NAME}/cache/${KEDR_PACKAGE_NAME}")
	else(KEDR_INSTALL_GLOBAL_IS_OPT)
		set(KEDR_PREFIX_CACHE
			"/var/cache/${KEDR_PACKAGE_NAME}")
	endif(KEDR_INSTALL_GLOBAL_IS_OPT)
else(KEDR_INSTALL_TYPE STREQUAL "global")
	set(KEDR_PREFIX_CACHE
		"${CMAKE_INSTALL_PREFIX}/var/cache/${KEDR_PACKAGE_NAME}")
endif(KEDR_INSTALL_TYPE STREQUAL "global")
# 10
if(KEDR_INSTALL_TYPE STREQUAL "global")
	if(KEDR_INSTALL_GLOBAL_IS_OPT)
		set(KEDR_PREFIX_VAR
			"/var/opt/${KEDR_PACKAGE_NAME}")
	else(KEDR_INSTALL_GLOBAL_IS_OPT)
		set(KEDR_PREFIX_VAR
			"/var/opt/${KEDR_PACKAGE_NAME}")
# Another variant
#		set(KEDR_PREFIX_VAR
#			"/var/${KEDR_PACKAGE_NAME}")
	endif(KEDR_INSTALL_GLOBAL_IS_OPT)
else(KEDR_INSTALL_TYPE STREQUAL "global")
	set(KEDR_PREFIX_VAR
		"${CMAKE_INSTALL_PREFIX}/var/${KEDR_PACKAGE_NAME}")
endif(KEDR_INSTALL_TYPE STREQUAL "global")
# 11
set(KEDR_INSTALL_PREFIX_DOC
	"${CMAKE_INSTALL_PREFIX}/share/doc/${KEDR_PACKAGE_NAME}")

# Set derivative prefixes

# additional, 1
set(KEDR_INSTALL_PREFIX_KMODULE "${KEDR_INSTALL_PREFIX_LIB}/modules/${KBUILD_VERSION_STRING}/misc")
# Another variant
#"${KEDR_INSTALL_PREFIX_LIB}/modules/${KBUILD_VERSION_STRING}/extra")
# additional, 2
set(KEDR_INSTALL_PREFIX_KSYMVERS "${CMAKE_INSTALL_PREFIX}/lib/modules/${KBUILD_VERSION_STRING}/symvers")
# additional, 3
set(KEDR_INSTALL_PREFIX_KINCLUDE
		"${KEDR_INSTALL_PREFIX_INCLUDE}")
# additional, 4
set(KEDR_INSTALL_PREFIX_EXAMPLES
		"${KEDR_INSTALL_PREFIX_READONLY}/examples")
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
#${CMAKE_SOURCE_DIR}/include - root of the include tree (see convensions).
include_directories("${CMAKE_SOURCE_DIR}/include")
kbuild_include_directories("${CMAKE_SOURCE_DIR}/include")

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
add_subdirectory(mist_gen)

# Directory for local mount debugfs for enable payloads tracepoints and for use by capturing trace script
# This directory is used in configuration file for control KEDR service.
set(kedr_debugfs_dir "/tmp/kedr/debugfs")

# This file will be used for tests, whether controller really intercept
# particular functions calls
set(function_triggers_file "${CMAKE_BINARY_DIR}/controller/tests/call_interception/function_triggers.data")
add_subdirectory(controller)

add_subdirectory(syscall_connector)
add_subdirectory(fault_simulation)

add_subdirectory(include)

#Configuration-files for collect payloads
set(kedr_cm_conf_filename "kedr_cm.conf")
set(kedr_cm_conf_file "${CMAKE_BINARY_DIR}/tools/control/${kedr_cm_conf_filename}")
set(kedr_test_cm_conf_filename "kedr_cm_test.conf")
set(kedr_test_cm_conf_file "${CMAKE_BINARY_DIR}/tools/control/${kedr_test_cm_conf_filename}")

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
