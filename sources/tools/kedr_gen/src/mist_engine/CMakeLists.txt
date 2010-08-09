cmake_minimum_required (VERSION 2.6)

#######################################################################
# Name of the project. 
# The primary programming language is C++, C is necessary for some 
# CMake-specific stuff.
project (mist_engine)
enable_language (C)

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
# Microsoft Windows-specific stuff
if (WIN32)
# Check if windres.exe and (optionally) lib.exe are available
    message (STATUS "Checking for windres.exe")
    find_program (WINDRES windres.exe)
    if (NOT WINDRES)
        message (FATAL_ERROR 
"windres.exe is not found (it is usually provided by MinGW)."
        )
    else ()
        message (STATUS "Checking for windres.exe: ${WINDRES}")
    endif ()
    
    message (STATUS "Checking for lib.exe")
    find_program (MS_LIB lib.exe)
    if (NOT MS_LIB)
        message (STATUS "Checking for lib.exe: not found.")
    else ()
        message (STATUS "Checking for lib.exe: ${MS_LIB}")
    endif ()

# Determine if the build environment is 32-bit or 64-bit. It is needed, 
# for instance, to build the Microsoft-style import library for MiST Engine.
# [NB] $(WIN32) is also true on 64-bit Windows systems.
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        message (STATUS 
"Checking if the build environment is 32-bit or 64-bit: 64-bit"
        )
        set (WIN_MACHINE_ARCH X64)

    elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
        message (STATUS 
"Checking if the build environment is 32-bit or 64-bit: 32-bit"
        )
        set (WIN_MACHINE_ARCH X86)

    else ()
# Only 32- and 64-bit build environments are currently supported
        message (FATAL_ERROR 
"Checking if the build environment is 32-bit or 64-bit: unknown"
        )
    endif ()

# Linux-specific stuff    
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set (MIST_OS_LINUX true)
    
else ()
# Neither Microsoft Windows nor Linux
    message (FATAL_ERROR 
"Only Linux and Microsoft Windows operating systems are currently supported"
    )
    
endif ()

#######################################################################
#message (STATUS 
#"The package is configured to be installed to ${CMAKE_INSTALL_PREFIX}"
#)

#######################################################################
# On Linux systems, check if the compiler supports -fvisibility option
# (necessary to build the shared library)
if (MIST_OS_LINUX)
    include (CheckCCompilerFlag)
    check_c_compiler_flag(-fvisibility=hidden MIST_GCC_HAS_FVISIBILITY)
endif ()

#######################################################################
# The version number.
set (MIST_ENGINE_VERSION_MAJOR 1)
set (MIST_ENGINE_VERSION_MINOR 0)
set (MIST_ENGINE_VERSION_MICRO 1)

set (MIST_ENGINE_VERSION 
"${MIST_ENGINE_VERSION_MAJOR}.${MIST_ENGINE_VERSION_MINOR}.${MIST_ENGINE_VERSION_MICRO}"
)

# X.Y.Z without any suffixes like release type, etc.
set (MIST_ENGINE_VERSION_PLAIN ${MIST_ENGINE_VERSION})

# MIST_ENGINE_RELEASE_TYPE should be "alpha", "beta", "final", an empty string
# (same meaning as "final"), etc.
if (MIST_ENGINE_RELEASE_TYPE)
    set (MIST_ENGINE_VERSION "${MIST_ENGINE_VERSION} ${MIST_ENGINE_RELEASE_TYPE}")
endif ()

#######################################################################
# Default bug report address
if (NOT MIST_ENGINE_BUGREPORT)
    set (MIST_ENGINE_BUGREPORT 
        "http://sourceforge.net/projects/template2code/develop"
    )
endif ()

#######################################################################
# Subdirectories to be processed
add_subdirectory (src)
#add_subdirectory (doc)

#######################################################################
# Packaging
#include (InstallRequiredSystemLibraries)
if (WIN32)
    set (CPACK_GENERATOR "ZIP")
else ()
    set (CPACK_GENERATOR "TBZ2")
endif ()

set (CPACK_PACKAGE_VERSION_MAJOR "${MIST_ENGINE_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${MIST_ENGINE_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${MIST_ENGINE_VERSION_MICRO}")

include (CPack)
#######################################################################
