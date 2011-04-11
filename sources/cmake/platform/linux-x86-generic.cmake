# This is a "toolchain" file for common Linux systems. 
# It should be used when building KEDR for another kernel on the same 
# x86 system.

########################################################################
# This toolchain file necessary to tell CMake we are going to perform 
# a cross-build (building for another system, "target system"). 
# When executing CMake, the path to this file should be specified in 
# -DCMAKE_TOOLCHAIN_FILE=...
#
# CMAKE_SYSTEM_VERSION should also be set then. It should contain the full 
# version string of the kernel, i.e., what "uname -r" would output on the 
# target system.
#
# Details: http://www.itk.org/Wiki/CMake_Cross_Compiling
#
# Mandatory variables: 
# - CMAKE_SYSTEM_NAME - it actually indicates we are going to cross-build;
# - KEDR_ROOT_DIR or KEDR_SYSTEM_MAP_FILE (at least one of there should be 
#   defined) - these variables are necessary to locate System.map file, 
#   see below.
########################################################################

set (KEDR_ROOT_DIR "/")

# When building for another system, System.map file for that system is used 
# to determine which functions are available (exported) there. 
# The default path to that file is 
# ${KEDR_ROOT_DIR}/boot/System.map-${KBUILD_VERSION_STRING}
# If you would like to use another path, please specify it in 
# KEDR_SYSTEM_MAP_FILE here
#set (KEDR_SYSTEM_MAP_FILE "some_path/boot/System.map")

set (CMAKE_SYSTEM_NAME "Linux")
