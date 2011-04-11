cmake_minimum_required (VERSION 2.6)

#######################################################################
# Name of the subproject and of the application
project (kedr_gen)
enable_language (C)
enable_language(CXX)

set (KEDR_GEN_APP ${PROJECT_NAME})

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
if (NOT KEDR_GEN_INSTALL_PREFIX)
	set (KEDR_GEN_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
endif ()

message (STATUS 
"\"${PROJECT_NAME}\" is configured to be installed to ${KEDR_GEN_INSTALL_PREFIX}"
)
#######################################################################
# Currently, only GNU C compilers are supported.
if (NOT CMAKE_COMPILER_IS_GNUCC)
    message (FATAL_ERROR 
"Only GNU C compiler is currently supported."
    )
endif ()

#######################################################################
# Make "Release" the default build type
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()
message (STATUS "${PROJECT_NAME}: Build type is \"${CMAKE_BUILD_TYPE}\"")

#######################################################################
set (MIST_BASE_NAME "mist_engine")

# Add the paths for the include files provided by mist_engine.
include_directories (
    "${CMAKE_CURRENT_BINARY_DIR}/${MIST_BASE_NAME}/src" 
    "${CMAKE_CURRENT_SOURCE_DIR}/${MIST_BASE_NAME}/src"
)

link_directories (
    "${CMAKE_CURRENT_BINARY_DIR}/${MIST_BASE_NAME}/src"
)

set (KEDR_GEN_SOURCES
    Common.cpp
    Generator.cpp
    TemplateLoader.cpp
    ValueLoader.cpp
    main.cpp
)
add_executable (${KEDR_GEN_APP} ${KEDR_GEN_SOURCES})
add_dependencies (${KEDR_GEN_APP} ${MIST_BASE_NAME}-shared)

target_link_libraries (${KEDR_GEN_APP} ${MIST_BASE_NAME})
set_target_properties(${KEDR_GEN_APP} PROPERTIES 
    INSTALL_RPATH "\$ORIGIN/mist_engine"
    BUILD_WITH_INSTALL_RPATH true
)

# Install "kedr_gen"
install (TARGETS ${KEDR_GEN_APP} DESTINATION ${KEDR_GEN_INSTALL_PREFIX})

# If it is not a temporary install (i.e. if kedr_gen is installed not for 
# building KEDR only), create a symlink to "kedr_gen" in the directory for
# executables (${prefix}/bin).
if (${KEDR_GEN_INSTALL_PREFIX} STREQUAL "${KEDR_INSTALL_PREFIX_EXEC_AUX}")
    # KEDR_INSTALL_PREFIX_EXEC must also be defined in this case
    if (NOT KEDR_INSTALL_PREFIX_EXEC)
        message (FATAL_ERROR 
"KEDR_INSTALL_PREFIX_EXEC is empty or is not defined."
        )
    endif()
    
    set (CMD_CREATE_SYMLINK 
"${CMAKE_COMMAND} -E create_symlink ${KEDR_INSTALL_PREFIX_EXEC_AUX}/${KEDR_GEN_APP} ${KEDR_INSTALL_PREFIX_EXEC}/${KEDR_GEN_APP}"
    )
    
    install (CODE "execute_process (COMMAND ${CMD_CREATE_SYMLINK})")
    
    # Instructions to uninstall the symlink when uninstalling KEDR
    add_custom_target (uninstall_kedr_gen_symlink
        rm -f "${KEDR_INSTALL_PREFIX_EXEC}/${KEDR_GEN_APP}"
    )
    add_dependencies (uninstall_files uninstall_kedr_gen_symlink)
endif ()


#######################################################################
# Subdirectories to be processed
add_subdirectory (mist_engine)
#######################################################################
