cmake_minimum_required(VERSION 2.6)

# Make "Release" the default build type
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo."
      FORCE)
endif ()
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")

# Default flags to compiler when build user-space programs.
#
# Should come before enabling language.
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra"
    CACHE STRING "Compiler flags for debug builds."
)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-g -Wall -Wextra"
    CACHE STRING "Compiler flags for Release With Debug Info builds."
)
set(CMAKE_C_FLAGS_RELEASE "-Wall"
    CACHE STRING "Compiler flags for release builds."
)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra"
    CACHE STRING "C++ compiler flags for debug builds."
)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -Wall -Wextra"
    CACHE STRING "C++ compiler flags for Release with Debug Info builds."
)
set(CMAKE_CXX_FLAGS_RELEASE "-Wall"
    CACHE STRING "C++ compiler flags for release builds."
)

enable_language(C)
enable_language(CXX)

project(kedr)
#######################################################################
# Prohibit a common type of an in-source build.
# Note that building in a subdirectory in the source tree is still allowed 
# as it can be convenient.
string (COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" in_source)
if (in_source)
    message (FATAL_ERROR 
"It is not allowed to build the project in its source directory. "
"Please delete CMakeCache.txt and CMakeFiles from ${CMAKE_SOURCE_DIR} "
"if they are there and then use some other directory to build KEDR."
)
endif () 

#######################################################################
# Names and versions
set(KEDR_PACKAGE_NAME "kedr")

set(KEDR_VERSION_MAJOR 0)
set(KEDR_VERSION_MINOR 5)
set(KEDR_VERSION_MICRO 0)
#set(KEDR_VERSION_SUFFIX "-devel" CACHE STRING
set(KEDR_VERSION_SUFFIX "" CACHE STRING
	"Version suffix, a string that should be appended to the version"
)
set(KEDR_VERSION 
"${KEDR_VERSION_MAJOR}.${KEDR_VERSION_MINOR}${KEDR_VERSION_SUFFIX}"
)

#######################################################################

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
include(CMakeParseArguments)

#######################################################################
include(path_prefixes)

# Set common install prefixes for KEDR
fill_install_prefixes(${KEDR_PACKAGE_NAME} # Package name
	KEDR # Prefix for variable names
	KERNEL # Set kernel-related prefixes also.
)

if(KEDR_INSTALL_TYPE STREQUAL "GLOBAL_OPT")
    message("Global installation into /opt")
elseif(KEDR_INSTALL_TYPE STREQUAL "GLOBAL")
    message("Global installation")
else(KEDR_INSTALL_TYPE STREQUAL "GLOBAL_OPT")
    message("Local installation")
endif(KEDR_INSTALL_TYPE STREQUAL "GLOBAL_OPT")
   

# Set additional(KEDR-specific) install prefixes
set(KEDR_DEFAULT_CONFIG_DIR "${KEDR_INSTALL_PREFIX_GLOBAL_CONF}")
set(KEDR_INSTALL_PREFIX_TEMPLATES "${KEDR_INSTALL_PREFIX_READONLY}/templates")
#######################################################################
# lsmod, rmmod, insmod and modprobe are used in may places.
# Instead of use guessed paths in every script where them needed,
# find these programs once.
set(modules_utils_paths "/sbin" "/bin" "/usr/bin")

find_program(LSMOD lsmod PATHS ${modules_utils_paths})
find_program(RMMOD rmmod PATHS ${modules_utils_paths})
find_program(INSMOD insmod PATHS ${modules_utils_paths})
find_program(MODPROBE modprobe PATHS ${modules_utils_paths})

if(NOT LSMOD OR NOT RMMOD OR NOT INSMOD OR NOT MODPROBE)
    message(SEND_ERROR "At least one of kernel module control command is not found(see above).")
    if(CMAKE_CROSSCOMPILING)
	message("In case of cross-compiling you may want to set CMAKE_FIND_ROOT_PATH to the root directory of the target environment")
    endif(CMAKE_CROSSCOMPILING)
endif(NOT LSMOD OR NOT RMMOD OR NOT INSMOD OR NOT MODPROBE)
#######################################################################
if(NOT USER_PART_ONLY)
    find_package(Kbuild 2.6.32)

    if(NOT Kbuild_ARCH STREQUAL "x86")
	message("Only x86 architecture is supported (this includes x86_64 machines also).")
	message(FATAL_ERROR "Unsupported architecture: ${Kbuild_ARCH}")
    endif(NOT Kbuild_ARCH STREQUAL "x86")
endif(NOT USER_PART_ONLY)
# TODO: Arhitecture may be checked for USER_PART_ONLY build.
# This would prevent its building for machine, which not support it
# (with any kernel).

include (multi_kernel)

if(USER_PART_ONLY OR KERNEL_PART_ONLY)
    # Check that multi-kernel build is allowed.
    if(NOT KEDR_INSTALL_TYPE MATCHES "GLOBAL")
	message(FATAL_ERROR "Multi-kernel build is not allowed for local installation type.")
    endif(NOT KEDR_INSTALL_TYPE MATCHES "GLOBAL")
endif(USER_PART_ONLY OR KERNEL_PART_ONLY)


include(template_generation)
include(cmake_useful)
#######################################################################
# Set default compiler flags for Kbuild.
if(KERNEL_PART)
    # Like normal compiler flags, these definitions should come before
    # including corresponded infrustructure.
    set(KBUILD_C_FLAGS_DEBUG "-g -DKEDR_DEBUG"
	CACHE STRING "Kbuild compiler flags for debug builds."
    )
    set(KBUILD_C_FLAGS_RELWITHDEBINFO "-g"
	CACHE STRING "Kbuild compiler flags for debug builds."
    )

    include(kbuild_system)

    include(kmodule)
endif(KERNEL_PART)
#######################################################################
if(KERNEL_PART)
    # [NB] Sparse is usually not installed by default. If you are going to
    # use it, make sure it is installed.
    option(KEDR_USE_SPARSE 
	"Use Sparse analysis tool when building kernel modules." 
	OFF
    )

    if(KEDR_USE_SPARCE)
	# Do not cache result, so user do not see C=1 flag appended.
	# Otherwise we need to precheck, whether flag is already set for cache value.
	set(KBUILD_MAKE_FLAGS "${KBUILD_MAKE_FLAGS} C=1")
    endif(KEDR_USE_SPARCE)
endif(KERNEL_PART)
#######################################################################
if(KERNEL_PART)
    # Adjust kernel-related prefixes to concrete kernel.
    kernel_path("${Kbuild_VERSION_STRING}" KEDR_INSTALL_PREFIX_KMODULE
	    "${KEDR_KERNEL_INSTALL_PREFIX_KMODULE}")
    kernel_path("${Kbuild_VERSION_STRING}" KEDR_INSTALL_PREFIX_KSYMVERS
	    "${KEDR_KERNEL_INSTALL_PREFIX_KSYMVERS}")
endif(KERNEL_PART)

#######################################################################
if(USER_PART)
    #  kedr_install_library(library_name)
    function(kedr_install_library library_name)
	    install(TARGETS ${library_name} LIBRARY
			    DESTINATION ${KEDR_INSTALL_PREFIX_LIB})
    endfunction(kedr_install_library library_name)
    #  kedr_install_headers(install_subdir header_file [..])
    function(kedr_install_headers install_subdir)
	    install(FILES ${header_file} ${ARGN}
			    DESTINATION ${KEDR_INSTALL_PREFIX_INCLUDE}/${install_subdir})
    endfunction(kedr_install_headers install_subdir)
endif(USER_PART)
if(KERNEL_PART)
    #  kedr_install_kmodule(kmodule_target)
    function(kedr_install_kmodule kmodule_target)
	kbuild_install(TARGETS ${kmodule_target}
	    MODULE DESTINATION "${KEDR_INSTALL_PREFIX_KMODULE}"
	)
    endfunction(kedr_install_kmodule kmodule_target)
    #  kedr_install_symvers(kmodule_target)
    function(kedr_install_symvers kmodule_target)
	kbuild_install(TARGETS ${kmodule_target}
	    SYMVERS DESTINATION "${KEDR_INSTALL_PREFIX_KSYMVERS}"
	)
    endfunction(kedr_install_symvers kmodule_target)
endif(KERNEL_PART)
#######################################################################
#  kedr_module_ref(RESULT_VAR module_name)
#
# Return reference to module, installed by kedr_install_kmodule().
# Used when form content of configuration files for "kedr" script.
#
#  kedr_module_load_command(RESULT_VAR module_name)
#
# Return string which contains command for load given module.
#
# Implementation of these functions depends build type.
#
# Note, while these function are intended to be used while configure
# shell scripts and configuration files, them also can be used for
# KERNEL_PART, when configured files are kernel-dependent.
if(KERNEL_PART AND USER_PART)
    # Single builds use full path to kernel modules and load them using
    # insmod.
    function(kedr_module_ref RESULT_VAR module_name)
	set(${RESULT_VAR} "${KEDR_INSTALL_PREFIX_KMODULE}/${module_name}.ko" PARENT_SCOPE)
    endfunction(kedr_module_ref)
    
    function(kedr_module_load_command RESULT_VAR module_name)
	set(${RESULT_VAR} "${INSMOD} ${KEDR_INSTALL_PREFIX_KMODULE}/${module_name}.ko" PARENT_SCOPE)
    endfunction(kedr_module_load_command)

else(KERNEL_PART AND USER_PART)
    # Multi-kernel builds use modprobe for load KEDR kernel modules.
    function(kedr_module_ref RESULT_VAR module_name)
	set(${RESULT_VAR} "${module_name}" PARENT_SCOPE)
    endfunction(kedr_module_ref)
    
    function(kedr_module_load_command RESULT_VAR module_name)
	# --first-time force modprobe to return nonzero if fails to load
	# module.
	set(${RESULT_VAR} "${MODPROBE} --first-time ${module_name}" PARENT_SCOPE)
    endfunction(kedr_module_load_command)
endif(KERNEL_PART AND USER_PART)
#######################################################################
include(uninstall_target)

if(USER_PART)
    # [NB] All the "prefix" directories ending with ${KEDR_PACKAGE_NAME} or
    # ${KEDR_PACKAGE_NAME}/share should be removed when uninstalling the 
    # package.
    add_uninstall_dir(
	    "${KEDR_INSTALL_PREFIX_EXEC_AUX}"
	    "${KEDR_INSTALL_PREFIX_READONLY}"
	    "${KEDR_INSTALL_PREFIX_GLOBAL_CONF}"
	    "${KEDR_INSTALL_PREFIX_LIB_AUX}"
	    "${KEDR_INSTALL_PREFIX_INCLUDE}"
	    "${KEDR_INSTALL_PREFIX_TEMP_SESSION}"
	    "${KEDR_INSTALL_PREFIX_TEMP}"
	    "${KEDR_INSTALL_PREFIX_STATE}"
	    "${KEDR_INSTALL_PREFIX_CACHE}"
	    "${KEDR_INSTALL_PREFIX_VAR}"
	    "${KEDR_INSTALL_PREFIX_DOC}"
	    "${KEDR_TEST_COMMON_PREFIX}"
    )
endif(USER_PART)
#######################################################################
if(KERNEL_PART)
    # Most include files are here
    kbuild_include_directories("${CMAKE_SOURCE_DIR}/include")
    # kedr/defs.h is configured into binary include dir.
    kbuild_include_directories("${CMAKE_BINARY_DIR}/include")
    # config.h is configured into top binary dir
    kbuild_include_directories("${CMAKE_BINARY_DIR}")
endif(KERNEL_PART)
#######################################################################
if(KERNEL_PART)
    # Perform basic checks

    # Check if kernel modules can be built on this system
    check_module_build()

    # Find out which memory allocator is used by the kernel. It is needed
    # because, for example, the order of arguments to kmem_cache_alloc*_trace()
    # may be different depending on the allocator.
    check_allocator()

    if (${KERNEL_MEMORY_ALLOCATOR} STREQUAL "slab" AND
	    Kbuild_VERSION_STRING_CLASSIC VERSION_LESS "3.7.0")
	    set (KMCA_TRACE_SIZE_FIRST 1)
    else ()
	    set (KMCA_TRACE_KMC_FIRST 1)
    endif ()

    # Check if stack trace information is reliable on this system.
    # STACK_TRACE_RELIABLE will hold the result.
    # This only checks relevant configuration parameters of kernel.
    check_stack_trace()

    # Check the signatures of hlist_for_each_entry*() macros, they changed 
    # in the kernel 3.9.
    check_hlist_for_each_entry()

    # Check if random32() is available.
    check_random32()
endif(KERNEL_PART)
#######################################################################
# Both user part and kernel part should be aware about
# call monitoring and fault simulation-related components.
option(KEDR_STANDARD_CALLM_PAYLOADS 
	"Enable the standard plugins (payload modules) for call monitoring." 
	OFF
)

option(KEDR_TRACE "Whether KEDR trace mechanism is built" ON)

if(KEDR_STANDARD_CALLM_PAYLOADS AND NOT KEDR_TRACE)
    message(FATAL_ERROR "Plagins for call monitoring are enabled without call monitoring mechanism implementation (KEDR_TRACE is OFF).")
endif(KEDR_STANDARD_CALLM_PAYLOADS AND NOT KEDR_TRACE)

if(KERNEL_PART AND KEDR_TRACE)
    # Check that KEDR trace mechanism can be implemented on this kernel.
    check_ring_buffer()

    if(NOT RING_BUFFER_IMPLEMENTED)
	# NB: We may not reject to configure with these parameters,
	# because them may be already used for build USER_SPACE_ONLY part.
	#
	# So we forcibly change parameters to ones which correct for kernel part.
	# Of course, disabled functionality cannot be used even it is
	# enabled in USER_PART.
	message("[WARNING] Tracing cannot be implemented for this kernel, so it will be disabled.")
	set(KEDR_TRACE OFF CACHE BOOL
	    "KEDR trace mechanism cannot be built for this kernel"
	    FORCE
	)
	if(KEDR_STANDARD_CALLM_PAYLOADS)
	    message("[WARNING] Plugins for call monitoring will be disabled too.")
	    set(KEDR_STANDARD_CALLM_PAYLOADS OFF CACHE BOOL
		"Standard plugins (payload modules) for call monitoring cannot be built for this kernel." 
		FORCE
	    )
	endif(KEDR_STANDARD_CALLM_PAYLOADS)
    endif(NOT RING_BUFFER_IMPLEMENTED)
endif(KERNEL_PART AND KEDR_TRACE)

option(KEDR_STANDARD_FSIM_PAYLOADS 
	"Enable the standard plugins (payload modules) for fault simulation." 
	ON
)
option(KEDR_LEAK_CHECK
	"Enable support for memory leak detection." 
	ON
)
#######################################################################

option(KEDR_ENABLE_CALLER_ADDRESS
	"Enable support for 'caller_address' variable in fault simulation indicators."
	ON
)
#######################################################################

# kedr_gen (this time - for building KEDR itself)
if (KEDR_GEN)
# If cross-compiling for a different architecture, kedr_gen tool 
# will probably not build here, so it should be built separately 
# in advance in this case. The path to kedr_gen should be passed here 
# in KEDR_GEN variable.

# Another case when KEDR_GEN normally is set is KERNEL_PART_ONLY build:
# you can set KEDR_GEN to path where USER_PART_ONLY build has already 
# installed it, that is <prefix>/lib/kedr/kedr_gen.

# ${KEDR_GEN_TOOL} is the path to "kedr_gen". The path can be used to 
# execute "kedr_gen" tool during the build of payload modules, etc.
	set (KEDR_GEN_TOOL ${KEDR_GEN})
	
	# Check if the tool exists at the path specified.
	execute_process (
	    COMMAND ${KEDR_GEN} 
	    RESULT_VARIABLE kedr_gen_exec_result
		OUTPUT_QUIET
	)
	if (NOT kedr_gen_exec_result EQUAL 0)
	    message (FATAL_ERROR 
	"${KEDR_GEN} does not exist or cannot be executed."
	    )
	endif ()
	
	message (STATUS "\"kedr_gen\" tool: ${KEDR_GEN}")

else (KEDR_GEN)
# kedr_gen is built here explicitly and installed to a temporary location.
# This is only necessary to build KEDR itself.
# This should be done before the first add_subdirectory() command.

# The temporary install directory must be persistent and it must not be 
# KEDR_INSTALL_PREFIX_TEMP because it has nothing to do with the 
# installation of KEDR: it is used when KEDR is being built only.

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

#Do not destroy the previously installed generator
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
endif (KEDR_GEN)

message (STATUS "Creating \"kedr_gen\" - done")

# Top directory with kedr_gen templates for different purposes.
set(KEDR_GEN_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/templates/") 
#######################################################################
if(KERNEL_PART)
    # This file will contain #defines that specify whether the particular 
    # functions are available and used. Keep these statements before 
    # "functions" subdirectory is processed.
    set(KEDR_FUNC_DEF_FILE "${CMAKE_BINARY_DIR}/func_def.h")

    # This is needed to generate func_def.h.
    set(KEDR_FUNC_DEF_FILE_IN "${CMAKE_BINARY_DIR}/func_def.h.in")

    # Clear the file, write the header guard
    file(WRITE "${KEDR_FUNC_DEF_FILE_IN}" 
	    "#ifndef KEDR_FUNC_DEF_1538_INCLUDED\n")
    file(APPEND "${KEDR_FUNC_DEF_FILE_IN}" 
	    "#define KEDR_FUNC_DEF_1538_INCLUDED\n\n")
    file(APPEND "${KEDR_FUNC_DEF_FILE_IN}" 
	    "/* Kernel functions to be processed by payload modules */\n\n")
endif(KERNEL_PART)
#######################################################################

if(KERNEL_PART)
    add_subdirectory(core)
endif(KERNEL_PART)

# Variables for use kedr core module
set(KEDR_CORE_NAME "kedr")
kedr_module_ref(KEDR_CORE_REF ${KEDR_CORE_NAME})
kedr_module_load_command(KEDR_CORE_LOAD_COMMAND ${KEDR_CORE_NAME})

if(KERNEL_PART)
    # Keep this before add_subdirectory() statements for payload modules.
    add_subdirectory(functions)
endif(KERNEL_PART)

if(USER_PART)
    add_subdirectory(templates)
endif(USER_PART)

if(KERNEL_PART)
    add_subdirectory(fault_simulation)
endif(KERNEL_PART)
# Variables for use kedr_fault_simulation module
set(KEDR_FAULT_SIMULATION_NAME "kedr_fault_simulation")
kedr_module_ref(KEDR_FAULT_SIMULATION_REF ${KEDR_FAULT_SIMULATION_NAME})
kedr_module_load_command(KEDR_FAULT_SIMULATION_LOAD_COMMAND ${KEDR_FAULT_SIMULATION_NAME})

add_subdirectory(include)

if(KEDR_TRACE)
    if(KERNEL_PART)
	add_subdirectory(trace)
    endif(KERNEL_PART)

    # Variables for use kedr_trace module
    set(KEDR_TRACE_NAME "kedr_trace")
    kedr_module_ref(KEDR_TRACE_REF ${KEDR_TRACE_NAME})
    kedr_module_load_command(KEDR_TRACE_LOAD_COMMAND ${KEDR_TRACE_NAME})
endif(KEDR_TRACE)

if(KEDR_STANDARD_FSIM_PAYLOADS)
    # List of fault simulation payload modules is collected
    # in GLOBAL PROPERTY FSIM_PAYLOADS.
    add_subdirectory(payloads_fsim)
    # List of fault simulation indicator modules is collected
    # in GLOBAL PROPERTY FSIM_INDICATORS.
    add_subdirectory(fault_indicators)
endif(KEDR_STANDARD_FSIM_PAYLOADS)


if(KEDR_STANDARD_CALLM_PAYLOADS)
    # List of call monitoring payload modules is collected
    # in GLOBAL PROPERTY CALLM_PAYLOADS.
    add_subdirectory(payloads_callm)
endif(KEDR_STANDARD_CALLM_PAYLOADS)

if(KEDR_LEAK_CHECK)
    # List of leak check payload modules is collected
    # in GLOBAL PROPERTY LC_PAYLOADS.
    add_subdirectory(leak_check)

    # Variables for use kedr_leak_check module
    set(KEDR_LEAK_CHECK_NAME "kedr_leak_check")
    kedr_module_ref(KEDR_LEAK_CHECK_REF ${KEDR_LEAK_CHECK_NAME})
    kedr_module_load_command(KEDR_LEAK_CHECK_LOAD_COMMAND ${KEDR_LEAK_CHECK_NAME})
endif(KEDR_LEAK_CHECK)

if(USER_PART)
    add_subdirectory(tools)
endif(USER_PART)


if (USER_PART AND NOT CMAKE_CROSSCOMPILING)
    # Examples
    include(example_system) # This will be use in testing also.
    add_subdirectory(examples)

    # Documentation
    add_subdirectory(doc)
endif (USER_PART AND NOT CMAKE_CROSSCOMPILING)
#######################################################################
if(KERNEL_PART)
    # After all payload modules have been processed, complete generation of
    # func_def.h
    file(APPEND "${KEDR_FUNC_DEF_FILE_IN}" 
	    "\n#endif /* KEDR_FUNC_DEF_1538_INCLUDED */\n")

    configure_file(
	    "${CMAKE_BINARY_DIR}/func_def.h.in"
	    "${CMAKE_BINARY_DIR}/func_def.h")
endif(KERNEL_PART)
#######################################################################
# Tests for package itself.
#
# Keep this after all other add_subdirectory() statements. One reason is
# that to configure the call interception tests, the complete list of 
# the kernel functions involved must be collected first when processing 
# the subdirectories for the payload modules.
option(WITH_TESTING "Whether need to build tests" OFF)
if(WITH_TESTING)
    add_subdirectory(tests)
endif(WITH_TESTING)
#######################################################################
if(KERNEL_PART)
    kbuild_finalize_linking()
endif(KERNEL_PART)
#######################################################################
# Configuration file is for kernel only.
if(KERNEL_PART)
    configure_file("${CMAKE_SOURCE_DIR}/config.h.in" "${CMAKE_BINARY_DIR}/config.h")
    message(STATUS "Creating config.h - done")
endif(KERNEL_PART)
#######################################################################
if(KERNEL_PART_ONLY)
    # For use modprobe on newly installed modules, depmod should be called.
    set(UPDATE_MODULES_UNINSTALL_AFTER uninstall_files)
    include(update_modules)
endif(KERNEL_PART_ONLY)
#######################################################################

message (STATUS "Configured ${KEDR_PACKAGE_NAME} version ${KEDR_VERSION}")
#######################################################################
