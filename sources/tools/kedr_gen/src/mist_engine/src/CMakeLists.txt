# [NB] .in-files are used both by Autotools and CMake, .cmake-files - 
# by CMake only

# Configure the header files to pass some of the CMake settings
# to the source code, etc.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/mist_engine.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/mist_engine.h"
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

if (WIN32)
# Resource definition file should be configured too.
    configure_file (
      "${CMAKE_CURRENT_SOURCE_DIR}/mist_engine.rc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/mist_engine.rc"
    )
endif()

# Additional compiler definitions. 
# Note that only GNU C compiler is currently supported, so the definitions
# are provided with this in mind. Whether the compiler is GNU compiler, it 
# should be checked in the top-level CMakeLists.txt.
add_definitions(-DHAVE_CONFIG_H)

if (WIN32)
# A crude way to handle the difference number of arguments that mkdir takes 
# on different platforms. A full-fledged detection would be better.
    add_definitions(-DMKDIR_ONE_ARG)
endif ()

if (MIST_OS_LINUX AND MIST_GCC_HAS_FVISIBILITY)
    add_definitions(-DGCC_HAS_FVISIBILITY)
endif ()

add_definitions(-std=gnu99 -Wall)

# Add the source and binary trees to the search path for include files.
include_directories ("${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

# The base part of the names of target files.
set (MIST_BASE_NAME ${PROJECT_NAME})

#######################################################################
# MiST Engine command line tool (built only if MiST Engine is packaged
# standalone rather than as a part of KEDR).
#######################################################################
if (NOT KEDR_PACKAGE_NAME)
    set (MIST_ENGINE_SOURCES
        main.c 
        grar.c 
        smap.c
        mist_base.c
        mist_file_utils.c 
        mist_string_utils.c 
        mist_exec.c 
    )
    add_executable(${MIST_BASE_NAME} ${MIST_ENGINE_SOURCES})
endif ()

#######################################################################
# MiST Engine shared library
#######################################################################
set (MIST_LIBRARY_TARGET ${MIST_BASE_NAME}-shared)
set (MIST_LIBRARY_SOURCES
    grar.c
    smap.c
    mist_base.c
    mist_file_utils.c
    mist_string_utils.c
    mist_engine.c
)

if (WIN32)
# Instructions to prepare and build the resource information into 
# the library on Windows.
    set (MIST_RSRC_OBJ ${MIST_BASE_NAME}-rsrc.obj)
    set (MIST_RC_FILE  "${CMAKE_CURRENT_BINARY_DIR}/mist_engine.rc")

    add_custom_command (
        OUTPUT  ${MIST_RSRC_OBJ}
        COMMAND ${WINDRES} ${MIST_RC_FILE} ${MIST_RSRC_OBJ}
        DEPENDS ${MIST_RC_FILE}
    )

    set_source_files_properties (${MIST_RSRC_OBJ} PROPERTIES
# this is an object file already, no need to compile
        EXTERNAL_OBJECT true
# the file has been generated during the build (this is important 
# for dependency checking)
        GENERATED true
    )
    
# Add the object file containing the resources to the list of files
# to build the DLL from.
    set (MIST_LIBRARY_SOURCES ${MIST_LIBRARY_SOURCES} ${MIST_RSRC_OBJ})
endif (WIN32)

if (MIST_OS_LINUX AND MIST_GCC_HAS_FVISIBILITY)
    set_source_files_properties (${MIST_LIBRARY_SOURCES} PROPERTIES
        COMPILE_FLAGS "-fvisibility=hidden"
    )
endif ()

add_library(${MIST_LIBRARY_TARGET} SHARED ${MIST_LIBRARY_SOURCES})
set_target_properties(${MIST_LIBRARY_TARGET} PROPERTIES 
    OUTPUT_NAME "${MIST_BASE_NAME}"
    COMPILE_DEFINITIONS "MIST_ENGINE_DLL_EXPORTS"
)

# Windows-specific stuff: import libraries in Microsoft and GCC formats, etc.
if (WIN32)
    set (MIST_DLL_FILE ${MIST_BASE_NAME}.dll)
    set (MIST_IMPLIB_GNU lib${MIST_BASE_NAME}.a)
    set (MIST_IMPLIB_MS  ${MIST_BASE_NAME}.lib)

# .def-file is necessary to create an import library compatible with
# Microsoft linker. 
# This file should be distributed even if that import library is not 
# to be created right now, it can be used later.
    set (MIST_DEF_FILE ${MIST_BASE_NAME}.def)

# Specify that the .def-file and the GCC-style import library 
# depend on the target that builds the shared library.
    add_custom_command (
        OUTPUT ${MIST_IMPLIB_GNU} ${MIST_DEF_FILE} 
        DEPENDS ${MIST_LIBRARY_TARGET}
    )

# Adjust the appropriate parts of the names of the libraries to be created
# to follow appropriate naming conventions.
    set_target_properties (${MIST_LIBRARY_TARGET} PROPERTIES 
        RUNTIME_OUTPUT_NAME "${MIST_BASE_NAME}"
        ARCHIVE_OUTPUT_NAME "${MIST_BASE_NAME}"
        PREFIX ""
        SUFFIX ".dll"
        IMPORT_PREFIX "lib"
        IMPORT_SUFFIX ".a"
# Additional linker options to produce a .def-file.
        LINK_FLAGS "-Wl,--output-def,${MIST_DEF_FILE}"
    )

# The list of import libraries for different linkers.
    set (MIST_IMPORT_LIBS ${MIST_IMPLIB_GNU})

# If Microsoft library manager (lib.exe) is available, prepare the import 
# library that can be used with Microsoft linker. Using the GNU-style import
# library with that linker can cause problems during linking and in runtime.
    if (MS_LIB)
        # Add the Microsoft-style import library to the list of import libraries.
        set (MIST_IMPORT_LIBS ${MIST_IMPORT_LIBS} ${MIST_IMPLIB_MS})

        # Prepare the Microsoft-style import library
        add_custom_command (
            OUTPUT  ${MIST_IMPLIB_MS}
            COMMAND ${MS_LIB} /nologo 
                -machine:${WIN_MACHINE_ARCH} 
                -name:${MIST_DLL_FILE} 
                -def:${MIST_DEF_FILE} 
                -out:${MIST_IMPLIB_MS}
            DEPENDS ${MIST_LIBRARY_TARGET} ${MIST_DEF_FILE} 
        )
    endif ()

# A custom target for import libraries (when it is processed, Microsoft-style 
# import library will be prepared if necessary).
    add_custom_target (import_libraries ALL 
        DEPENDS ${MIST_IMPORT_LIBS}
    )

    # Let the DLL go to 'bin' directory too (sometimes this can be convenient)
    install (TARGETS ${MIST_LIBRARY_TARGET}
        RUNTIME DESTINATION lib
    )
    
    # Install the import libraries and the .def-file.
    install (FILES 
    "${CMAKE_CURRENT_BINARY_DIR}/${MIST_IMPLIB_GNU}"
    "${CMAKE_CURRENT_BINARY_DIR}/${MIST_DEF_FILE}"
        DESTINATION lib
    )

    if (MS_LIB)
        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${MIST_IMPLIB_MS}"
            DESTINATION lib
        )
    endif ()
endif (WIN32)

# Linux-specific stuff
if (MIST_OS_LINUX)
    set (VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/versions.ldscript")

    set_target_properties (${MIST_LIBRARY_TARGET} PROPERTIES
        SOVERSION ${MIST_ENGINE_VERSION_MAJOR}
        VERSION ${MIST_ENGINE_VERSION_PLAIN}
        LINK_FLAGS "-Wl,-z,defs	-Wl,--version-script=${VERSION_SCRIPT}"
    )
endif (MIST_OS_LINUX)

#######################################################################
# Installation: common part

if (KEDR_GEN_INSTALL_PREFIX)
    # When installing MiST Engine as a part of KEDR, we only need the 
    # shared libraries.
    
    set (KEDR_GEN_INSTALL_MIST_PREFIX 
        "${KEDR_GEN_INSTALL_PREFIX}/${MIST_BASE_NAME}")
    
    # Install the libraries
    install (TARGETS ${MIST_LIBRARY_TARGET}
        RUNTIME DESTINATION ${KEDR_GEN_INSTALL_MIST_PREFIX}
        LIBRARY DESTINATION ${KEDR_GEN_INSTALL_MIST_PREFIX}
    )
else ()
    # Install the command line tool
    install (TARGETS ${MIST_BASE_NAME} DESTINATION bin)

    # Install the libraries
    install (TARGETS ${MIST_LIBRARY_TARGET}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
    )

    # Install the header files
    install (FILES 
    "${CMAKE_CURRENT_BINARY_DIR}/mist_engine.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/mist_errors.h"
        DESTINATION include/${MIST_BASE_NAME}
    )
endif ()
