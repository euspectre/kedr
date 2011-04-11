# The following variables are set here:
#
# KBUILD_VERSION_STRING - `uname -r` (${CMAKE_SYSTEM_VERSION})
# KBUILD_BUILD_DIR - directory, where the modules are built 
#     (often /lib/modules/${KBUILD_VERSION_STRING}/build)
# KBUILD_INCLUDE_DIR - not used
# KBUILD_FOUND - TRUE if everything is correct, FALSE otherwise

if (NOT KBUILD_VERSION_STRING)
	set(KBUILD_VERSION_STRING ${CMAKE_SYSTEM_VERSION} CACHE STRING 
		"Kernel version for which KEDR is built."
	)
endif (NOT KBUILD_VERSION_STRING)

if (NOT KBUILD_BUILD_DIR)
	set(KBUILD_BUILD_DIR "/lib/modules/${KBUILD_VERSION_STRING}/build")
endif (NOT KBUILD_BUILD_DIR)

set(KBUILD_INCLUDE_DIRS "NOT USED")

# Note: only KBUILD_BUILD_DIR variable is really used in the project.
# Other variables defined only for FindModule architecture of CMake.

# Handle the QUIETLY and REQUIRED arguments and set KBUILD_FOUND to TRUE if 
# all listed variables are TRUE

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Kbuild DEFAULT_MSG KBUILD_BUILD_DIR)
