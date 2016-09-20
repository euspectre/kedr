set(PLUGIN_NAME "kedr-i13n")
#######################################################################

# GCC version >= 5.3 is required.
if (NOT CMAKE_COMPILER_IS_GNUCC OR
	CMAKE_C_COMPILER_VERSION VERSION_LESS 5.3)
	message(FATAL_ERROR "GCC 5.3 or newer is required.")
endif ()
#######################################################################

# Check if the compiler supports -fvisibility option (necessary to build
# the shared library)
include(CheckCCompilerFlag)
check_c_compiler_flag(-fvisibility=hidden GCC_HAS_FVISIBILITY)
if (NOT GCC_HAS_FVISIBILITY)
	message(FATAL_ERROR
"This version of GCC does not support \'-fvisibility=hidden\' option.")
endif ()
#######################################################################

# the plugin itself
add_subdirectory(plugin)

# the common target module - for debugging and testing
add_subdirectory(tests/common_target)
#######################################################################

# the sources of the stubs
install(FILES
	kedr_stubs.c
	DESTINATION "${KEDR_DATADIR}")
#######################################################################
