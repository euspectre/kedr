# Note. The packagers might want to build the components of KEDR separately
# and place them in separate packages:
#
# * the GCC plugin works on the machine where the target kernel or modules
#   are built;
# * the core kernel module works on the machine where the target kernel or
#   modules run.
#
# cmake is expected to be called at the top level of the build tree.
#
# If 'make' is called there after that, it will build everything for the
# current system. As a part of this process, the relevant kernel modules
# will be built for the currently running kernel.
#
# There is no uninstall support here yet.
############################################################################
#
# Notes for the packagers
# The packagers might want to do things as follows instead:
#
# * Run cmake as usual, probably with -DCMAKE_INSTALL_PREFIX=/usr
#   Consider specifying LIB_INSTALL_DIR and SHARE_INSTALL_PREFIX
#   explicitly too, if their default values are not acceptable.
#
# * Run make in the directories for the user-space components
#   (currently: i13n). Then run make install there, with appropriate
#   DESTDIR.
#
# * Copy the sources of the kernel modules somewhere. That is, everything
#   from "kernel" subdir in the source tree except CMakeLists.txt and
#   *.in files.
#   After CMake has configured the files, copy kernel/config.h and
#   kernel/Makefile.mk from the build tree to that location too.
#   Then you can prepare DKMS / akmod / whatever else packages for the
#   kernel modules from the files you have copied.
#######################################################################

cmake_minimum_required(VERSION 3.4)
#######################################################################

project(kedr)
#######################################################################

enable_language(C)
enable_language(CXX)
#######################################################################

# The version.
set(KEDR_VERSION_MAJOR 1)
set(KEDR_VERSION_MINOR 0)
# Version suffix, a string that should be appended to the version.
# Use "-devel" for the development builds, "" otherwise.
set(KEDR_VERSION_SUFFIX "-devel")

set(KEDR_VERSION
"${KEDR_VERSION_MAJOR}.${KEDR_VERSION_MINOR}${KEDR_VERSION_SUFFIX}")
#######################################################################

# Prohibit a common type of an in-source build.
# Note that building in a subdirectory in the source tree is still allowed
# and can often be convenient.
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" in_source)
if (in_source)
	message (FATAL_ERROR
"It is not allowed to build the project in its source directory. "
"Please delete CMakeCache.txt and CMakeFiles from ${CMAKE_SOURCE_DIR} "
"if they are there and then use some other directory to build KEDR."
)
endif ()
#######################################################################

# Make "Release" the default build type.
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING
	  "The build type, one of \"Debug\", \"Release\", \"RelWithDebInfo\"."
	  FORCE)
endif ()
message (STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"")

# Set the install paths, if they are not set already.
if (NOT CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_PREFIX "/usr/local")
endif ()

if (NOT LIB_INSTALL_DIR)
	set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib64")
endif ()

if (NOT SHARE_INSTALL_PREFIX)
	set(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share")
endif ()

# Location of the KEDR-specific libraries and plugins
if (NOT KEDR_LIBDIR)
	set(KEDR_LIBDIR "${LIB_INSTALL_DIR}/kedr")
endif ()

# Location of the read-only data files.
if (NOT KEDR_DATADIR)
	set(KEDR_DATADIR "${SHARE_INSTALL_PREFIX}/kedr")
endif ()
#######################################################################

# Default compiler flags
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
#######################################################################

# It might be convenient to build everything except the GCC plugin in some
# cases, at least for debugging, etc.
option(KEDR_I13N
	"Build the instrumentation facilities."
	ON)
#######################################################################

# Specify which kinds of analysis KEDR should be able to perform.
# These options only control what is built. When using KEDR, set the
# appropriate options in runtime to enable the desired analyzers.
option(KEDR_LEAK_CHECKER
	"Build the checker for memory leaks."
	ON)

option(KEDR_RACE_DETECTOR
	"Build the data race detector."
	OFF)

if (NOT KEDR_LEAK_CHECKER AND NOT KEDR_RACE_DETECTOR)
	message(FATAL_ERROR
		"At least one kind of analysis should be enabled in KEDR.")
endif ()
#######################################################################

# ARCH and CROSS_COMPILE variables that can be passed to make when building
# kernel modules.
if (ARCH)
	set(ARCH_SPEC " ARCH=${ARCH}")
	set(HDR_ARCH "${ARCH}")
else ()
	set(ARCH_SPEC "")
	set(HDR_ARCH "")
endif ()

if (CROSS_COMPILE)
	set(CROSS_COMPILE_SPEC " CROSS_COMPILE=${CROSS_COMPILE}")
else ()
	set(CROSS_COMPILE_SPEC "")
endif ()
#######################################################################

# Prepare config-related definitions.
# DEFINE_* are for the header file, CONFIG_* are for Kbuilds and Makefiles.
if (KEDR_LEAK_CHECKER)
	set(DEFINE_KEDR_LEAK_CHECKER
	    "#define CONFIG_KEDR_LEAK_CHECKER 1")
	set(CONFIG_KEDR_LEAK_CHECKER
	    "export CONFIG_KEDR_LEAK_CHECKER=y")
else ()
	set(DEFINE_KEDR_LEAK_CHECKER
	    "#undef CONFIG_KEDR_LEAK_CHECKER")
	set(CONFIG_KEDR_LEAK_CHECKER
	    "# CONFIG_KEDR_LEAK_CHECKER is not set")
endif ()

if (KEDR_RACE_DETECTOR)
	set(DEFINE_KEDR_RACE_DETECTOR
	    "#define CONFIG_KEDR_RACE_DETECTOR 1")
	set(CONFIG_KEDR_RACE_DETECTOR
	    "export CONFIG_KEDR_RACE_DETECTOR=y")
else ()
	set(DEFINE_KEDR_RACE_DETECTOR
	    "#undef CONFIG_KEDR_RACE_DETECTOR")
	set(CONFIG_KEDR_RACE_DETECTOR
	    "# CONFIG_KEDR_RACE_DETECTOR is not set")
endif ()
#######################################################################

add_subdirectory(kernel)

if (KEDR_I13N)
	add_subdirectory(i13n)
endif ()
#######################################################################

# MODULE_DIR is for informational purpose only. The actual location to
# store kernel modules is determined in kernel/Makefile.mk.in.
set(MODULE_DIR "/lib/modules/<kernel_version>/extra/kedr/")
if (NOT ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
	set(MODULE_DIR
		"${CMAKE_INSTALL_PREFIX}${MODULE_DIR}")
endif ()

message("")
message("Paths:\n"
        "\tInstallation prefix:           ${CMAKE_INSTALL_PREFIX}\n"
	"\tLibraries and GCC plugins:     ${KEDR_LIBDIR}\n"
	"\tData:                          ${KEDR_DATADIR}\n"
	"\tThe location of the kernel modules depends on the target kernel version:\n"
	"\t  ${MODULE_DIR}")

message("")
message("Components:")
if (KEDR_I13N)
	message("\tInstrumentation facilities: enabled")
else ()
	message("\tInstrumentation facilities: disabled")
endif ()
if (KEDR_LEAK_CHECKER)
	message("\tMemory leak checker: enabled")
else ()
	message("\tMemory leak checker: disabled")
endif ()
if (KEDR_RACE_DETECTOR)
	message("\tData race detector:  enabled")
else ()
	message("\tData race detector:  disabled")
endif ()
message("")

message (STATUS "Configured project \"${PROJECT_NAME}\" version ${KEDR_VERSION}")
#######################################################################
