# Set variables:
#
# KBUILD_VERSION_STRING - `uname -r` (${CMAKE_SYSTEM_VERSION})
# KBUILD_BUILD_DIR  -   directory, where module are build (/lib/modules/${KBUILD_VERSION_STRING}/build)
# KBUILD_INCLUDE_DIR - /usr/src/linux-headers-${KBUILD_VERSION_STRING}/include
# KBUILD_FOUND - TRUE if all correct, FALSE otherwise

set(KBUILD_VERSION_STRING "${CMAKE_SYSTEM_VERSION}")
set(KBUILD_BUILD_DIR "/lib/modules/${KBUILD_VERSION_STRING}/build")
set(KBUILD_INCLUDE_DIRS "/usr/src/linux-headers-${KBUILD_VERSION_STRING}/include")

# Handle the QUIETLY and REQUIRED arguments and set KBUILD_FOUND to TRUE if 
# all listed variables are TRUE

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Kbuild DEFAULT_MSG KBUILD_BUILD_DIR)